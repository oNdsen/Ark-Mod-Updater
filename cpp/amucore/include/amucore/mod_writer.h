#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace amucore {

struct ModMap {
  std::string name;  // map name without trailing null (as stored in the .mod, with null re-added)
  int32_t size;      // length-prefix from mod.info (includes the trailing null)
};

struct ParsedModInfo {
  std::vector<ModMap> maps;
};

// Parse a mod.info blob. Layout (little-endian): int32 firstStringSize, then at
// firstStringSize+4 an int32 map count, then at firstStringSize+8 each map as
// int32 size + char[size] (null-terminated). Mirrors _GetMaps in inc/MakeMod.au3.
ParsedModInfo parseModInfo(const std::vector<uint8_t>& bytes);

// Build an ARK .mod file. `modmeta` is the raw modmeta.info blob; pass empty to use
// the constant fallback. Mirrors _CreateModFile in inc/MakeMod.au3 (byte-for-byte).
std::vector<uint8_t> buildModFile(uint64_t modId, const ParsedModInfo& info,
                                  const std::vector<uint8_t>& modmeta);

}  // namespace amucore
