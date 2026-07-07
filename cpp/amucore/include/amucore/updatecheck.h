#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Self-update support for the C++ AMU. Unlike the single-exe AutoIt app, the
// package is multi-file (amu.exe + sciter.dll + ui/*), so the update channel is
// manifest-based and hash-verified (the "hardened updater" plan):
//
//   https://ondsen.ch/files/amu/v2/version.txt    plain version, e.g. "0.10.0"
//   https://ondsen.ch/files/amu/v2/manifest.txt   one file per line:
//                                                 <relpath>|<sha256 lowercase hex>|<size>
//   https://ondsen.ch/files/amu/v2/<relpath>      the file contents
//
// (v2 subfolder so the legacy AutoIt channel at /files/amu/ stays untouched.
// publish.ps1 produces the whole folder from a build.) The in-app check reads
// version.txt and compares against amucore::version(); the standalone
// updater.exe downloads every manifest file, verifies each SHA-256, then swaps
// all files with rollback and relaunches amu.exe. Everything here is shared by
// both sides and unit-testable except the two network/file helpers.

namespace amucore {

struct ManifestFile {
  std::string path;    // relative, forward or backslashes ("ui/main.html")
  std::string sha256;  // lowercase hex, 64 chars
  int64_t size = 0;
};

struct Manifest {
  std::string version;
  std::vector<ManifestFile> files;
};

// PURE: parse manifest.txt content. Lines: "<path>|<sha256>|<size>"; blank
// lines and lines starting with '#' are skipped; malformed lines are dropped.
// `version` is carried into the result (it comes from version.txt).
Manifest parseManifest(const std::string& text, const std::string& version);

// PURE: dotted-numeric version compare ("0.10.1" vs "0.9.9"): negative when
// a < b, 0 when equal, positive when a > b. Missing segments count as 0; any
// non-numeric suffix in a segment is ignored ("0.10.0-dev" -> 0.10.0).
int compareVersions(const std::string& a, const std::string& b);

// WinHTTP GET https://<host><path> -> body as a string; "" on any failure
// (network, non-2xx). Small text payloads only (version/manifest).
std::string httpGetText(const std::wstring& host, const std::wstring& path);

// WinHTTP GET https://<host><path> streamed to `destFile` (binary). Returns
// false on any failure; a partial file may remain and must be discarded by
// the caller. `destFile` is UTF-8.
bool httpDownloadFile(const std::wstring& host, const std::wstring& path,
                      const std::string& destFile);

// SHA-256 of a file via CNG/BCrypt, lowercase hex; "" on failure.
std::string sha256File(const std::string& path);

// The fixed update channel: GitHub Releases of the public AMU repo. The
// "latest/download/<asset>" URL always serves the newest release's asset
// (GitHub answers with a redirect to objects.githubusercontent.com - WinHTTP
// follows https->https redirects by default). Release assets are FLAT, so
// manifest relpaths are flattened into asset names via updateAssetName().
inline constexpr const wchar_t* kUpdateHost = L"github.com";
inline constexpr const wchar_t* kUpdateBasePath =
    L"/oNdsen/ark-mod-updater/releases/latest/download/";

// "ui/main.html" -> "ui_main.html": GitHub release assets have no directories,
// so publish.ps1 uploads flattened names and the updater requests them the
// same way (staging/install paths keep the original relpath).
inline std::string updateAssetName(const std::string& relpath) {
  std::string out = relpath;
  for (char& c : out)
    if (c == '/' || c == '\\') c = '_';
  return out;
}

}  // namespace amucore
