#include "amucore/credstore.h"

#include <cstdint>
#include <vector>

#include <windows.h>
#include <dpapi.h>  // CryptProtectData / CryptUnprotectData

namespace amucore {

namespace {

constexpr char kPrefix[] = "DPAPI:";
constexpr size_t kPrefixLen = 6;  // strlen("DPAPI:")

// Uppercase hex, matching AutoIt's String(binary) => "0x" + UPPERCASE hex digits.
std::string toHex(const std::vector<uint8_t>& bytes) {
  static const char kDigits[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(bytes.size() * 2 + 2);
  out += "0x";
  for (uint8_t b : bytes) {
    out.push_back(kDigits[(b >> 4) & 0x0F]);
    out.push_back(kDigits[b & 0x0F]);
  }
  return out;
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Parse "0x<HEX>" (or bare "<HEX>") into bytes. Returns false on any malformed input
// (non-hex digit or odd digit count), mirroring AutoIt's Binary() being fed the
// substring after "DPAPI:". Case-insensitive on the "0x" marker and digits.
bool fromHex(const std::string& s, std::vector<uint8_t>& out) {
  size_t i = 0;
  if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) i = 2;
  const size_t digits = s.size() - i;
  if (digits == 0 || (digits % 2) != 0) return false;
  out.clear();
  out.reserve(digits / 2);
  for (; i < s.size(); i += 2) {
    const int hi = hexVal(s[i]);
    const int lo = hexVal(s[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return true;
}

}  // namespace

std::string protect(const std::string& plaintext) {
  if (plaintext.empty()) return "";

  DATA_BLOB in;
  in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
  in.cbData = static_cast<DWORD>(plaintext.size());

  DATA_BLOB out{};
  if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
    return plaintext;  // graceful: degrade to plaintext
  }

  std::vector<uint8_t> blob(out.pbData, out.pbData + out.cbData);
  if (out.pbData) LocalFree(out.pbData);

  return std::string(kPrefix) + toHex(blob);
}

std::string unprotect(const std::string& stored) {
  if (stored.empty()) return "";
  if (stored.compare(0, kPrefixLen, kPrefix) != 0) {
    return stored;  // legacy plaintext value, returned as-is
  }

  std::vector<uint8_t> blob;
  if (!fromHex(stored.substr(kPrefixLen), blob) || blob.empty()) {
    return stored;  // malformed hex: graceful
  }

  DATA_BLOB in;
  in.pbData = blob.data();
  in.cbData = static_cast<DWORD>(blob.size());

  DATA_BLOB out{};
  if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
    return stored;  // corrupt/foreign blob: graceful, don't crash
  }

  std::string plain(reinterpret_cast<char*>(out.pbData),
                    static_cast<size_t>(out.cbData));
  if (out.pbData) LocalFree(out.pbData);
  return plain;
}

}  // namespace amucore
