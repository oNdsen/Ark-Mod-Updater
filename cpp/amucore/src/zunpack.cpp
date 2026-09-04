#include "amucore/zunpack.h"

#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include <zlib.h>

namespace amucore {

namespace {
constexpr uint64_t kSignature = 2653586369ull;  // 6-byte sig + 2-byte format version

// Sanity bounds for the sizes read from the (untrusted) header. DEFLATE cannot
// expand by more than 1032:1, so an honest archive never claims more than this
// multiple of its own file size; the absolute cap keeps a large-but-hostile file
// from asking for an allocation that would throw on a worker thread. Both are far
// above any real ARK mod file (chunks are 128 KiB, whole files tens of MB).
constexpr uint64_t kMaxExpansionRatio = 1100;
constexpr uint64_t kMaxUnpackedSize = 4ull * 1024 * 1024 * 1024;  // 4 GiB

uint64_t readU64LE(const uint8_t* p, size_t off) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(p[off + i]) << (8 * i);
  return v;
}

uint64_t readU64LE(const std::vector<uint8_t>& b, size_t off) { return readU64LE(b.data(), off); }

// Largest unpacked size worth believing for an input of `inSize` bytes.
uint64_t plausibleUnpackedLimit(size_t inSize) {
  const uint64_t n = static_cast<uint64_t>(inSize);
  if (n > kMaxUnpackedSize / kMaxExpansionRatio) return kMaxUnpackedSize;
  return n * kMaxExpansionRatio;
}

// Mirrors _UnPack in inc/UnpackZx64.au3. Header: 4x uint64 (signature, unpacked chunk
// size, packed full size, unpacked size); then (compressed,uncompressed) uint64 pairs
// until the uncompressed sizes total the unpacked size; then the compressed chunks.
// EVERY size below comes straight from the file and is therefore attacker-controlled:
// no addition on those values may wrap, and none may be cast down unchecked.
std::vector<uint8_t> unpackZImpl(const std::vector<uint8_t>& in, int* err) {
  auto fail = [&](int e) {
    if (err) *err = e;
    return std::vector<uint8_t>{};
  };

  constexpr size_t kHeader = 32;
  if (in.size() < kHeader) return fail(2);
  if (readU64LE(in, 0) != kSignature) return fail(2);

  const uint64_t unpackedChunkSize = readU64LE(in, 8);
  const uint64_t unpackedSize = readU64LE(in, 24);
  // Every allocation below is bounded by unpackedSize, so bound that one first.
  if (unpackedSize > plausibleUnpackedLimit(in.size())) return fail(4);

  std::vector<std::pair<uint64_t, uint64_t>> index;  // (compressed, uncompressed)
  uint64_t sizeIndexed = 0;
  size_t i = 0;  // counts uint64 slots consumed from the index
  while (sizeIndexed < unpackedSize) {
    if (kHeader + (i + 2) * 8 > in.size()) return fail(4);
    const uint64_t compressed = readU64LE(in, kHeader + i * 8);
    const uint64_t uncompressed = readU64LE(in, kHeader + (i + 1) * 8);
    // Subtract instead of adding: a huge entry must not wrap sizeIndexed around
    // into an accidental match with unpackedSize. This also keeps every single
    // uncompressed value <= unpackedSize, i.e. within the cap checked above.
    if (uncompressed > unpackedSize - sizeIndexed) return fail(4);
    sizeIndexed += uncompressed;
    index.emplace_back(compressed, uncompressed);
    i += 2;
  }
  if (sizeIndexed != unpackedSize) return fail(4);

  size_t dataOff = kHeader + i * 8;  // <= in.size(), enforced by the loop above
  std::vector<uint8_t> out;
  out.reserve(static_cast<size_t>(unpackedSize));
  size_t read = 0;
  for (const auto& [compressed, uncompressed] : index) {
    // Compare with subtraction: "dataOff + compressed > in.size()" wraps mod 2^64
    // and lets a hostile size through, after which uncompress() reads far past
    // the buffer. zlib's uLong/uLongf are 32-bit on MSVC, so both values must
    // also be proven to survive the cast.
    if (dataOff > in.size() || compressed > in.size() - dataOff) return fail(4);
    if (compressed > static_cast<uint64_t>((std::numeric_limits<uLong>::max)()) ||
        uncompressed > static_cast<uint64_t>((std::numeric_limits<uLongf>::max)())) {
      return fail(4);
    }
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
}  // namespace

int64_t zHeaderUnpackedSize(const uint8_t* p, size_t n) {
  if (p == nullptr || n < 32) return -1;
  if (readU64LE(p, 0) != kSignature) return -1;
  const uint64_t size = readU64LE(p, 24);  // header field: unpacked full size
  // A value with the top bit set would come back as a negative "size".
  if (size > static_cast<uint64_t>((std::numeric_limits<int64_t>::max)())) return -1;
  return static_cast<int64_t>(size);
}

std::vector<uint8_t> unpackZ(const std::vector<uint8_t>& in, int* err) {
  // The caps in unpackZImpl make a hostile allocation implausible, but a genuinely
  // huge archive can still exhaust memory. Callers run this on worker threads with
  // no handler of their own, so report an allocation failure as code 4 instead of
  // letting the exception terminate the process.
  try {
    return unpackZImpl(in, err);
  } catch (const std::bad_alloc&) {
    if (err) *err = 4;
    return {};
  } catch (const std::length_error&) {
    if (err) *err = 4;
    return {};
  }
}

}  // namespace amucore
