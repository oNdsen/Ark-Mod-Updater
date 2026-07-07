#include "amucore/steamcmd_parser.h"

#include <algorithm>
#include <cctype>

namespace amucore {

namespace {

constexpr size_t kNpos = std::string::npos;

// Digit run immediately after `key` (skipping spaces). Empty if not found.
std::string numberAfter(const std::string& s, const char* key) {
  size_t p = s.find(key);
  if (p == kNpos) return {};
  p += std::char_traits<char>::length(key);
  while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
  std::string n;
  while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p]))) n.push_back(s[p++]);
  return n;
}

// The digit run right before the word "bytes" (e.g. "(1024 bytes)"). 0 if absent.
long long bytesBeforeWord(const std::string& s) {
  size_t b = s.find("bytes");
  if (b == kNpos) return 0;
  size_t end = b;
  while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
  size_t start = end;
  while (start > 0 && std::isdigit(static_cast<unsigned char>(s[start - 1]))) --start;
  if (start == end) return 0;
  return std::stoll(s.substr(start, end - start));
}

}  // namespace

bool SteamCmdParser::parseLine(const std::string& lineIn, SteamCmdEvent& out) {
  std::string line = lineIn;
  line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
  if (line.empty()) return false;

  if (line.find("Success") != kNpos && line.find("Downloaded item") != kNpos) {
    const std::string id = numberAfter(line, "Downloaded item");
    if (!id.empty()) {
      out = SteamCmdEvent{};
      out.type = SteamCmdEvent::Type::DownloadSucceeded;
      out.modId = id;
      out.bytes = bytesBeforeWord(line);
      return true;
    }
  }

  if (line.find("ERROR!") != kNpos && line.find("Download item") != kNpos) {
    const std::string id = numberAfter(line, "Download item");
    if (!id.empty()) {
      out = SteamCmdEvent{};
      out.type = SteamCmdEvent::Type::DownloadFailed;
      out.modId = id;
      const size_t op = line.find('(', line.find("failed"));
      const size_t cp = (op == kNpos) ? kNpos : line.find(')', op);
      if (op != kNpos && cp != kNpos && cp > op)
        out.reason = line.substr(op + 1, cp - op - 1);
      out.removed = out.reason.find("File Not Found") != kNpos;
      return true;
    }
  }

  if (line.find("Downloading item") != kNpos) {
    const std::string id = numberAfter(line, "Downloading item");
    if (!id.empty()) {
      out = SteamCmdEvent{};
      out.type = SteamCmdEvent::Type::Downloading;
      out.modId = id;
      return true;
    }
  }

  return false;
}

std::vector<SteamCmdEvent> SteamCmdParser::feed(const char* data, size_t len) {
  std::vector<SteamCmdEvent> events;
  buf_.append(data, len);
  size_t pos;
  while ((pos = buf_.find('\n')) != kNpos) {
    const std::string line = buf_.substr(0, pos);
    buf_.erase(0, pos + 1);
    SteamCmdEvent ev;
    if (parseLine(line, ev)) events.push_back(ev);
  }
  return events;
}

std::vector<SteamCmdEvent> SteamCmdParser::flush() {
  std::vector<SteamCmdEvent> events;
  if (!buf_.empty()) {
    SteamCmdEvent ev;
    if (parseLine(buf_, ev)) events.push_back(ev);
    buf_.clear();
  }
  return events;
}

}  // namespace amucore
