#pragma once

#include <cstdint>
#include <vector>

namespace amucore {

// Decompress a Valve/ARK ".z" archive blob to its raw bytes. On failure returns an
// empty vector and sets *err (if non-null) to a code mirroring inc/UnpackZx64.au3:
//   2 = bad signature/version, 4 = header/index mismatch (or truncated),
//   5 = a chunk's decompressed size != the index, 6 = more than one partial chunk.
std::vector<uint8_t> unpackZ(const std::vector<uint8_t>& in, int* err = nullptr);

// Read the total unpacked size from a .z archive header (the 4th uint64 of the
// 32-byte header). Returns -1 when fewer than 32 bytes are supplied or the
// signature does not match. Lets the orchestrator's _GetUnpackedModSize port
// estimate a mod's installed size by reading only 32 bytes per .z file.
int64_t zHeaderUnpackedSize(const uint8_t* p, size_t n);

}  // namespace amucore
