#include "amucore/updatecheck.h"

#include <cstddef>
#include <limits>

namespace amucore {

namespace {

constexpr size_t kNpos = std::string::npos;

// Trim spaces, tabs and CR from both ends (manifest lines may end in \r\n).
std::string trim(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
  return s.substr(b, e - b);
}

// Validate a 64-char hex SHA-256 and normalize it to lowercase. Returns false
// (leaving `out` untouched) when the input is not a plausible digest.
bool normalizeSha256(const std::string& in, std::string* out) {
  if (in.size() != 64) return false;
  std::string norm;
  norm.reserve(64);
  for (char c : in) {
    if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
    const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!hex) return false;
    norm.push_back(c);
  }
  *out = norm;
  return true;
}

// Strictly numeric decimal -> int64. Returns false on empty/non-digit input and
// on anything too large for int64 - the manifest is downloaded, so an absurd
// digit run must not overflow the accumulator (signed overflow is UB).
bool parseSize(const std::string& s, int64_t* out) {
  if (s.empty()) return false;
  constexpr int64_t kMax = (std::numeric_limits<int64_t>::max)();
  int64_t v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    const int d = c - '0';
    if (v > (kMax - d) / 10) return false;
    v = v * 10 + d;
  }
  *out = v;
  return true;
}

// Numeric prefix of the dotted version segment starting at *i; advances *i past
// the digits. Saturates at kMaxSegment rather than overflowing int64 on an absurd
// digit run - a saturated segment still compares as "very large", so ordering
// stays sensible for anything remotely version-shaped.
int64_t segmentValue(const std::string& s, size_t* i) {
  constexpr int64_t kMaxSegment = 999999999999999999LL;  // 18 digits
  int64_t v = 0;
  while (*i < s.size() && s[*i] >= '0' && s[*i] <= '9') {
    const int d = s[*i] - '0';
    v = (v <= (kMaxSegment - d) / 10) ? v * 10 + d : kMaxSegment;
    ++*i;
  }
  return v;
}

}  // namespace

Manifest parseManifest(const std::string& text, const std::string& version) {
  Manifest m;
  m.version = version;

  size_t start = 0;
  while (start <= text.size()) {
    const size_t nl = text.find('\n', start);
    std::string line = (nl == kNpos) ? text.substr(start)
                                     : text.substr(start, nl - start);
    line = trim(line);

    if (!line.empty() && line[0] != '#') {
      // Exactly three '|'-separated fields: <relpath>|<sha256 hex>|<size>.
      const size_t p1 = line.find('|');
      const size_t p2 = (p1 == kNpos) ? kNpos : line.find('|', p1 + 1);
      const bool threeFields =
          p2 != kNpos && line.find('|', p2 + 1) == kNpos;
      if (threeFields) {
        ManifestFile f;
        f.path = trim(line.substr(0, p1));
        const std::string hash = trim(line.substr(p1 + 1, p2 - p1 - 1));
        const std::string size = trim(line.substr(p2 + 1));
        if (!f.path.empty() && normalizeSha256(hash, &f.sha256) &&
            parseSize(size, &f.size)) {
          m.files.push_back(std::move(f));
        }
      }
      // Malformed lines are silently dropped by design.
    }

    if (nl == kNpos) break;
    start = nl + 1;
  }
  return m;
}

int compareVersions(const std::string& a, const std::string& b) {
  size_t ia = 0;
  size_t ib = 0;
  while (ia < a.size() || ib < b.size()) {
    // Numeric prefix of the current dotted segment; missing segments are 0 and
    // any non-numeric suffix ("-dev", "rc1") is ignored.
    const int64_t va = segmentValue(a, &ia);
    const int64_t vb = segmentValue(b, &ib);
    if (va != vb) return va < vb ? -1 : 1;

    // Skip the rest of the segment up to and including the next dot.
    while (ia < a.size() && a[ia] != '.') ++ia;
    if (ia < a.size()) ++ia;
    while (ib < b.size() && b[ib] != '.') ++ib;
    if (ib < b.size()) ++ib;
  }
  return 0;
}

HttpsUrl splitHttpsUrl(const std::string& url) {
  HttpsUrl out;
  static const char kScheme[] = "https://";
  const size_t kSchemeLen = sizeof(kScheme) - 1;
  if (url.size() <= kSchemeLen) return out;
  for (size_t i = 0; i < kSchemeLen; ++i) {
    char c = url[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != kScheme[i]) return out;  // http://, ftp://, javascript:, ...
  }

  size_t hostEnd = url.find_first_of("/?#", kSchemeLen);
  if (hostEnd == kNpos) hostEnd = url.size();
  const std::string host = url.substr(kSchemeLen, hostEnd - kSchemeLen);
  if (host.empty()) return out;
  // Host names only: '@' (userinfo) and ':' (port) would make the request go
  // somewhere other than <host>:443, which is all the helpers can do.
  for (unsigned char c : host) {
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '.' || c == '-' ||
                         c == '_';
    if (!allowed) return out;
  }

  std::string path = (hostEnd < url.size()) ? url.substr(hostEnd) : std::string();
  const size_t hash = path.find('#');  // fragment is client-side only
  if (hash != kNpos) path.resize(hash);
  if (path.empty() || path[0] != '/') path.insert(path.begin(), '/');
  // Spaces, CR/LF and other control bytes have no business in a request line.
  for (unsigned char c : path)
    if (c <= 0x20 || c == 0x7F) return out;

  out.ok = true;
  out.host = host;
  out.path = path;
  return out;
}

}  // namespace amucore

#ifdef _WIN32

#include <windows.h>

#include <bcrypt.h>
#include <winhttp.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace amucore {

namespace {

std::wstring utf8ToWide(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                    static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return {};
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                      w.data(), n);
  return w;
}

// Shared WinHTTP GET over https://<host><path>. Streams every body chunk into
// `sink(data, len)`; the whole request fails on any WinHTTP error, a non-2xx
// status, or a sink that returns false. Mirrors workshop.cpp's getModInfo.
template <typename Sink>
bool httpGet(const std::wstring& host, const std::wstring& path, Sink&& sink) {
  HINTERNET hSession = WinHttpOpen(
      L"AMU/2 (Windows; update check)", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return false;
  // resolve/connect 15s, send/receive 30s - a hung CDN must not stall forever.
  WinHttpSetTimeouts(hSession, 15000, 15000, 30000, 30000);

  HINTERNET hConnect =
      WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return false;
  }

  HINTERNET hRequest =
      WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                         WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr);

  if (ok) {
    DWORD status = 0;
    DWORD len = sizeof(status);
    ok = WinHttpQueryHeaders(
             hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
             WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
             WINHTTP_NO_HEADER_INDEX) &&
         status >= 200 && status < 300;
  }

  if (ok) {
    for (;;) {
      DWORD avail = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
        ok = false;
        break;
      }
      if (avail == 0) break;  // end of body
      std::vector<char> chunk(avail);
      DWORD read = 0;
      if (!WinHttpReadData(hRequest, chunk.data(), avail, &read)) {
        ok = false;
        break;
      }
      if (read == 0) break;
      if (!sink(chunk.data(), read)) {
        ok = false;
        break;
      }
    }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return ok;
}

}  // namespace

std::string httpGetText(const std::wstring& host, const std::wstring& path) {
  std::string body;
  const bool ok = httpGet(host, path, [&](const char* data, DWORD len) {
    body.append(data, len);
    return true;
  });
  return ok ? body : std::string();
}

bool httpDownloadFile(const std::wstring& host, const std::wstring& path,
                      const std::string& destFile) {
  std::ofstream out(std::filesystem::path(utf8ToWide(destFile)),
                    std::ios::binary | std::ios::trunc);
  if (!out) return false;

  bool ok = httpGet(host, path, [&](const char* data, DWORD len) {
    out.write(data, static_cast<std::streamsize>(len));
    return static_cast<bool>(out);
  });

  out.close();
  return ok && !out.fail();
}

std::string sha256File(const std::string& path) {
  std::ifstream in(std::filesystem::path(utf8ToWide(path)), std::ios::binary);
  if (!in) return {};

  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) <
      0) {
    return {};
  }

  std::string hex;
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
    bool ok = true;
    std::vector<char> buf(64 * 1024);
    while (ok && in) {
      in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
      const std::streamsize got = in.gcount();
      if (got > 0) {
        ok = BCryptHashData(hash,
                            reinterpret_cast<PUCHAR>(buf.data()),
                            static_cast<ULONG>(got), 0) >= 0;
      }
    }
    if (ok && in.bad()) ok = false;  // read error (eof alone is fine)

    UCHAR digest[32] = {};
    if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0) {
      static const char kHexDigits[] = "0123456789abcdef";
      hex.reserve(64);
      for (UCHAR byte : digest) {
        hex.push_back(kHexDigits[byte >> 4]);
        hex.push_back(kHexDigits[byte & 0x0F]);
      }
    }
    BCryptDestroyHash(hash);
  }

  BCryptCloseAlgorithmProvider(alg, 0);
  return hex;
}

}  // namespace amucore

#endif  // _WIN32
