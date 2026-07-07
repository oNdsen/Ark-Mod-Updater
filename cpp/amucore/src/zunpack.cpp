#include "amucore/zunpack.h"

#include <utility>

#include <zlib.h>

namespace amucore {

namespace {
constexpr uint64_t kSignature = 2653586369ull;  // 6-byte sig + 2-byte format version

uint64_t readU64LE(const uint8_t* p, size_t off) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(p[off + i]) << (8 * i);
  return v;
}

uint64_t readU64LE(const std::vector<uint8_t>& b, size_t off) { return readU64LE(b.data(), off); }
}  // namespace

int64_t zHeaderUnpackedSize(const uint8_t* p, size_t n) {
  if (p == nullptr || n < 32) return -1;
  if (readU64LE(p, 0) != kSignature) return -1;
  return static_cast<int64_t>(readU64LE(p, 24));  // header field: unpacked full size
}

// Mirrors _UnPack in inc/UnpackZx64.au3. Header: 4x uint64 (signature, unpacked chunk
// size, packed full size, unpacked size); then (compressed,uncompressed) uint64 pairs
// until the uncompressed sizes total the unpacked size; then the compressed chunks.
std::vector<uint8_t> unpackZ(const std::vector<uint8_t>& in, int* err) {
  auto fail = [&](int e) {
    if (err) *err = e;
    return std::vector<uint8_t>{};
  };

  constexpr size_t kHeader = 32;
  if (in.size() < kHeader) return fail(2);
  if (readU64LE(in, 0) != kSignature) return fail(2);

  const uint64_t unpackedChunkSize = readU64LE(in, 8);
  const uint64_t unpackedSize = readU64LE(in, 24);

  std::vector<std::pair<uint64_t, uint64_t>> index;  // (compressed, uncompressed)
  uint64_t sizeIndexed = 0;
  size_t i = 0;  // counts uint64 slots consumed from the index
  while (sizeIndexed < unpackedSize) {
    if (kHeader + (i + 2) * 8 > in.size()) return fail(4);
    const uint64_t compressed = readU64LE(in, kHeader + i * 8);
    const uint64_t uncompressed = readU64LE(in, kHeader + (i + 1) * 8);
    sizeIndexed += uncompressed;
    index.emplace_back(compressed, uncompressed);
    i += 2;
  }
  if (sizeIndexed != unpackedSize) return fail(4);

  size_t dataOff = kHeader + i * 8;
  std::vector<uint8_t> out;
  out.reserve(static_cast<size_t>(unpackedSize));
  size_t read = 0;
  for (const auto& [compressed, uncompressed] : index) {
    if (dataOff + compressed > in.size()) return fail(4);
    std::vector<uint8_t> chunk(static_cast<size_t>(uncompressed));
    uLongf destLen = static_cast<uLongf>(uncompressed);
    const int zr = uncompress(chunk.data(), &destLen, in.data() + dataOff,
                              static_cast<uLong>(compressed));
    if (zr != Z_OK || destLen != uncompressed) return fail(5);
    out.insert(out.end(), chunk.begin(), chunk.end());
    dataOff += static_cast<size_t>(compressed);
    ++read;
    if (uncompressed != unpackedChunkSize && read != index.size()) return fail(6);
  }

  if (err) *err = 0;
  return out;
}

}  // namespace amucore
