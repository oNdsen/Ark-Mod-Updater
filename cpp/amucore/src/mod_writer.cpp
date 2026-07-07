#include "amucore/mod_writer.h"

namespace amucore {

namespace {
int32_t readI32LE(const std::vector<uint8_t>& b, size_t off) {
  if (off + 4 > b.size()) return 0;
  return static_cast<int32_t>(static_cast<uint32_t>(b[off]) |
                              (static_cast<uint32_t>(b[off + 1]) << 8) |
                              (static_cast<uint32_t>(b[off + 2]) << 16) |
                              (static_cast<uint32_t>(b[off + 3]) << 24));
}
}  // namespace

ParsedModInfo parseModInfo(const std::vector<uint8_t>& b) {
  ParsedModInfo out;
  if (b.size() < 8) return out;

  const int32_t firstStringSize = readI32LE(b, 0);
  if (firstStringSize < 0) return out;
  const int32_t numMaps = readI32LE(b, static_cast<size_t>(firstStringSize) + 4);

  size_t idx = static_cast<size_t>(firstStringSize) + 8;
  for (int i = 0; i < numMaps; ++i) {
    if (idx + 4 > b.size()) break;
    const int32_t sz = readI32LE(b, idx);
    idx += 4;
    if (sz < 0 || idx + static_cast<size_t>(sz) > b.size()) break;
    std::string name;
    for (int k = 0; k < sz; ++k) {
      const char c = static_cast<char>(b[idx + k]);
      if (c == '\0') break;  // AutoIt char[] stops at the first null
      name.push_back(c);
    }
    out.maps.push_back({name, sz});
    idx += static_cast<size_t>(sz);
  }
  return out;
}

std::vector<uint8_t> buildModFile(uint64_t modId, const ParsedModInfo& info,
                                  const std::vector<uint8_t>& modmeta) {
  std::vector<uint8_t> o;
  auto u32 = [&](uint32_t v) {
    o.push_back(static_cast<uint8_t>(v & 0xFF));
    o.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    o.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    o.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
  };
  auto u64 = [&](uint64_t v) {
    for (int i = 0; i < 8; ++i) o.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
  };
  auto str = [&](const std::string& s) { o.insert(o.end(), s.begin(), s.end()); };

  // Determine ModType byte + payload (mirrors the $BinNotModMeta fallback).
  std::vector<uint8_t> meta = modmeta;
  uint8_t iType;
  if (meta.empty()) {
    static const uint8_t kFallback[] = {
        0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 'M', 'o', 'd',
        'T',  'y',  'p',  'e',  0x00, 0x02, 0x00, 0x00, 0x00, '1', 0x00};
    meta.assign(kFallback, kFallback + sizeof(kFallback));
    iType = 0x01;
  } else {
    const std::string ms(meta.begin(), meta.end());
    iType = (ms.find("ModType") != std::string::npos) ? 0x00 : 0x01;
  }

  u64(modId);                    // mod id (8-byte LE uint64)
  u32(8);                        // constant
  str("ModName");                // literal + null
  o.push_back(0x00);
  u32(1);                        // constant 0x01 (written as int32)
  o.push_back(0x00);             // Chr(0) before map count
  u32(static_cast<uint32_t>(info.maps.size()));
  for (const auto& m : info.maps) {
    u32(static_cast<uint32_t>(m.size));
    str(m.name);
    o.push_back(0x00);
  }
  u32(0xFF22FF33u);              // constant (fits int32 -> 4 bytes: 33 ff 22 ff)
  u32(2);                        // constant
  o.push_back(iType);
  o.insert(o.end(), meta.begin(), meta.end());
  return o;
}

}  // namespace amucore
