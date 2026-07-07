#pragma once

#include <string>
#include <vector>

namespace amucore {

// One meaningful event parsed from steamcmd's stdout.
struct SteamCmdEvent {
  enum class Type { DownloadSucceeded, DownloadFailed, Downloading };
  Type type;
  std::string modId;
  long long bytes = 0;   // set for DownloadSucceeded
  std::string reason;    // set for DownloadFailed
  bool removed = false;  // DownloadFailed with "File Not Found" (removed/private item)
};

// Line-buffering parser for steamcmd stdout. Feed arbitrary byte chunks (steamcmd's
// output is not aligned to lines); complete lines are parsed and emitted. Call flush()
// at process end to handle a final line printed without a trailing newline.
// Mirrors _ParseSteamCMDLine in amu.au3 (hand-parsed, no std::regex).
class SteamCmdParser {
 public:
  std::vector<SteamCmdEvent> feed(const char* data, size_t len);
  std::vector<SteamCmdEvent> feed(const std::string& s) {
    return feed(s.data(), s.size());
  }
  std::vector<SteamCmdEvent> flush();

 private:
  bool parseLine(const std::string& line, SteamCmdEvent& out);
  std::string buf_;
};

}  // namespace amucore
