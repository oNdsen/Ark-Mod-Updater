#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <vector>

#include "amucore/zunpack.h"

using namespace amucore;

static std::vector<uint8_t> readFileZ(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

TEST_CASE(".z unpack matches the AutoIt golden output byte-for-byte") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto z = readFileZ(dir + "/sample.z");
  const auto golden = readFileZ(dir + "/sample.unpacked");
  REQUIRE(z.size() == 442);
  REQUIRE(golden.size() == 858);

  int err = -1;
  const auto out = unpackZ(z, &err);
  CHECK(err == 0);
  CHECK(out.size() == golden.size());
  CHECK(out == golden);
}

TEST_CASE(".z unpack rejects a bad signature") {
  std::vector<uint8_t> bad(64, 0);
  int err = 0;
  const auto out = unpackZ(bad, &err);
  CHECK(out.empty());
  CHECK(err == 2);
}

// ---------------------------------------------------------------------------
// Hostile headers. Every size in a .z header is attacker-controlled, so each of
// these used to either read past the input buffer or throw out of a worker
// thread; all of them must now come back as failure code 4.
// ---------------------------------------------------------------------------

static void putU64(std::vector<uint8_t>& b, uint64_t v) {
  for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

// 32-byte header + one (compressed,uncompressed) index pair + `payload` zero bytes.
static std::vector<uint8_t> makeZ(uint64_t unpackedSize, uint64_t compressed,
                                  uint64_t uncompressed, size_t payload) {
  std::vector<uint8_t> z;
  putU64(z, 2653586369ull);  // signature + format version
  putU64(z, 131072);         // unpacked chunk size (the real ARK value)
  putU64(z, 16);             // packed full size (not used by the reader)
  putU64(z, unpackedSize);
  putU64(z, compressed);
  putU64(z, uncompressed);
  z.resize(z.size() + payload, 0);
  return z;
}

TEST_CASE(".z unpack rejects a compressed size that overflows the bounds check") {
  // dataOff + compressed wraps mod 2^64 to a value below in.size(), so the old
  // check passed and uncompress() was handed (uLong)0xFFFFFFFF of the input.
  const auto z = makeZ(100, 0xFFFFFFFFFFFFFFFFull, 100, 16);
  int err = 0;
  const auto out = unpackZ(z, &err);
  CHECK(out.empty());
  CHECK(err == 4);
}

TEST_CASE(".z unpack rejects a compressed size that runs past the end of the input") {
  const auto z = makeZ(100, 4096, 100, 16);  // only 16 payload bytes are present
  int err = 0;
  const auto out = unpackZ(z, &err);
  CHECK(out.empty());
  CHECK(err == 4);
}

TEST_CASE(".z unpack rejects an implausible unpacked size instead of throwing") {
  // 1 EiB claimed by a 64-byte file: reserve()/vector() used to throw
  // std::length_error straight out of the worker thread.
  const auto z = makeZ(1ull << 60, 8, 1ull << 60, 16);
  int err = 0;
  const auto out = unpackZ(z, &err);
  CHECK(out.empty());
  CHECK(err == 4);
}

TEST_CASE(".z unpack rejects an index whose chunk sizes wrap around the total") {
  // The running total wraps mod 2^64 back onto the claimed 100 bytes, so the
  // "sizeIndexed != unpackedSize" check accepts an index holding a 2^64-sized
  // chunk. The index must be rejected outright (code 4) rather than accepted and
  // only incidentally failing later in the decompression loop (code 5).
  std::vector<uint8_t> z;
  putU64(z, 2653586369ull);
  putU64(z, 131072);
  putU64(z, 24);
  putU64(z, 100);                                 // unpacked full size
  putU64(z, 8); putU64(z, 60);                    // chunk 1: total 60
  putU64(z, 8); putU64(z, 0xFFFFFFFFFFFFFFCEull); // chunk 2: 2^64-50 -> wraps to 10
  putU64(z, 8); putU64(z, 90);                    // chunk 3: lands back on 100
  z.resize(z.size() + 32, 0);

  int err = 0;
  const auto out = unpackZ(z, &err);
  CHECK(out.empty());
  CHECK(err == 4);
}

TEST_CASE("zHeaderUnpackedSize rejects a size that does not fit in int64") {
  // The raw cast used to turn this into INT64_MIN, i.e. a huge negative "size".
  const auto z = makeZ(1ull << 63, 8, 8, 0);
  CHECK(zHeaderUnpackedSize(z.data(), z.size()) == -1);
}

TEST_CASE("zHeaderUnpackedSize still reads the real fixture header") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto z = readFileZ(dir + "/sample.z");
  REQUIRE(z.size() >= 32);
  CHECK(zHeaderUnpackedSize(z.data(), z.size()) == 858);
}
