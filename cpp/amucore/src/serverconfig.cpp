#include "amucore/serverconfig.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>
#include <fstream>

namespace amucore {

namespace {

// Trim ASCII whitespace from both ends, matching AutoIt StringStripWS with
// $STR_STRIPLEADING|$STR_STRIPTRAILING (the effective part of $STR_STRIPALL for
// numeric ids, which contain no interior whitespace).
std::string trim(const std::string& s) {
  auto isws = [](unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
  };
  size_t a = 0, b = s.size();
  while (a < b && isws(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && isws(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

// UTF-8 -> UTF-16 for Win32 wide APIs.
std::wstring toWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

// UTF-16 -> UTF-8.
std::string toUtf8(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                              nullptr, nullptr);
  if (n <= 0) return std::string();
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                      nullptr);
  return s;
}

}  // namespace

std::vector<std::string> parseActiveMods(const std::string& value) {
  std::vector<std::string> ids;
  size_t start = 0;
  while (start <= value.size()) {
    size_t comma = value.find(',', start);
    std::string part =
        (comma == std::string::npos) ? value.substr(start) : value.substr(start, comma - start);
    std::string id = trim(part);
    if (!id.empty()) ids.push_back(id);
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return ids;
}

std::string joinActiveMods(const std::vector<std::string>& ids) {
  std::string out;
  for (const auto& raw : ids) {
    std::string id = trim(raw);
    if (id.empty()) continue;
    if (!out.empty()) out.push_back(',');
    out += id;
  }
  return out;
}

std::vector<std::string> addModId(const std::vector<std::string>& ids, const std::string& id,
                                  bool* added) {
  const std::string want = trim(id);
  std::vector<std::string> out = ids;
  for (const auto& e : ids) {
    if (trim(e) == want) {
      if (added) *added = false;
      return out;  // already active -> unchanged
    }
  }
  out.push_back(want);
  if (added) *added = true;
  return out;
}

std::vector<std::string> removeModId(const std::vector<std::string>& ids, const std::string& id,
                                     bool* removed) {
  const std::string want = trim(id);
  std::vector<std::string> out;
  bool any = false;
  for (const auto& e : ids) {
    const std::string t = trim(e);
    if (t.empty()) continue;  // drop empties, as the AutoIt rebuild loop does
    if (t == want) {
      any = true;
      continue;
    }
    out.push_back(t);
  }
  if (removed) *removed = any;
  return out;
}

std::vector<std::string> readActiveMods(const std::string& iniPath) {
  const std::wstring wPath = toWide(iniPath);
  const std::wstring wSection = toWide(kServerSettingsSection);
  const std::wstring wKey = toWide(kActiveModsKey);

  // ActiveMods can be a long comma list; grow the buffer until it fits.
  std::wstring buf;
  DWORD cap = 1024;
  for (;;) {
    buf.assign(cap, L'\0');
    DWORD n = GetPrivateProfileStringW(wSection.c_str(), wKey.c_str(), L"", buf.data(), cap,
                                       wPath.c_str());
    // A truncated result fills cap-1 chars; grow and retry.
    if (n < cap - 1) {
      buf.resize(n);
      break;
    }
    if (cap >= (1u << 20)) {  // 1M wchars is far beyond any real ActiveMods line
      buf.resize(n);
      break;
    }
    cap *= 2;
  }
  return parseActiveMods(toUtf8(buf));
}

bool writeActiveMods(const std::string& iniPath, const std::vector<std::string>& ids) {
  const std::wstring wPath = toWide(iniPath);
  const std::wstring wSection = toWide(kServerSettingsSection);
  const std::wstring wKey = toWide(kActiveModsKey);
  const std::wstring wValue = toWide(joinActiveMods(ids));
  return WritePrivateProfileStringW(wSection.c_str(), wKey.c_str(), wValue.c_str(),
                                    wPath.c_str()) != 0;
}

bool addActiveMod(const std::string& iniPath, const std::string& id) {
  bool added = false;
  std::vector<std::string> next = addModId(readActiveMods(iniPath), id, &added);
  if (!added) return false;  // already active: AutoIt returns without IniWrite
  writeActiveMods(iniPath, next);
  return true;
}

bool removeActiveMod(const std::string& iniPath, const std::string& id) {
  bool removed = false;
  std::vector<std::string> next = removeModId(readActiveMods(iniPath), id, &removed);
  // _RemoveSelectedMod always IniWrites the rebuilt list, even if the id was
  // absent, so we mirror that (normalizes whitespace/empties). Return value
  // reports whether an entry was actually dropped.
  writeActiveMods(iniPath, next);
  return removed;
}

std::string iniRead(const std::string& iniPath, const std::string& section,
                    const std::string& key, const std::string& def) {
  const std::wstring wPath = toWide(iniPath);
  const std::wstring wSection = toWide(section);
  const std::wstring wKey = toWide(key);
  const std::wstring wDef = toWide(def);
  // Grow the buffer until the value fits (values can be long, e.g. MOTD).
  std::wstring buf;
  DWORD cap = 512;
  for (;;) {
    buf.assign(cap, L'\0');
    DWORD n = GetPrivateProfileStringW(wSection.c_str(), wKey.c_str(), wDef.c_str(), buf.data(),
                                       cap, wPath.c_str());
    if (n < cap - 1) {
      buf.resize(n);
      break;
    }
    if (cap >= (1u << 20)) {
      buf.resize(n);
      break;
    }
    cap *= 2;
  }
  return toUtf8(buf);
}

bool iniWrite(const std::string& iniPath, const std::string& section, const std::string& key,
              const std::string& value) {
  const std::wstring wPath = toWide(iniPath);
  const std::wstring wSection = toWide(section);
  const std::wstring wKey = toWide(key);
  const std::wstring wValue = toWide(value);
  return WritePrivateProfileStringW(wSection.c_str(), wKey.c_str(), wValue.c_str(),
                                    wPath.c_str()) != 0;
}

namespace {

// Fetch a double-NUL-terminated block from a profile API that reports
// truncation as "returned cap-2". `fetch(buf, cap)` is either
// GetPrivateProfileSectionNamesW or GetPrivateProfileSectionW bound to its
// section/path arguments. Grows the buffer until the block fits (start 4096
// wchars, give up growing at ~1M wchars).
template <typename Fetch>
std::wstring readProfileBlock(Fetch fetch) {
  std::wstring buf;
  DWORD cap = 4096;
  for (;;) {
    buf.assign(cap, L'\0');
    DWORD n = fetch(buf.data(), cap);
    if (n != cap - 2 || cap >= (1u << 20)) {
      buf.resize(n);
      return buf;
    }
    cap *= 2;
  }
}

// Split a double-NUL-terminated block ("a\0b\0\0") into its strings. An empty
// block yields no elements.
std::vector<std::wstring> splitDoubleNul(const std::wstring& block) {
  std::vector<std::wstring> out;
  size_t pos = 0;
  while (pos < block.size()) {
    size_t nul = block.find(L'\0', pos);
    if (nul == std::wstring::npos) nul = block.size();
    if (nul == pos) break;  // empty string == the terminating second NUL
    out.emplace_back(block, pos, nul - pos);
    pos = nul + 1;
  }
  return out;
}

}  // namespace

std::vector<IniSection> iniReadAll(const std::string& iniPath) {
  const std::wstring wPath = toWide(iniPath);
  std::vector<IniSection> sections;

  // Section list. GetPrivateProfileSectionNamesW returns the copied length and
  // reports a too-small buffer as cap-2. Missing file -> empty block.
  const std::wstring nameBlock = readProfileBlock([&wPath](wchar_t* buf, DWORD cap) {
    return GetPrivateProfileSectionNamesW(buf, cap, wPath.c_str());
  });

  for (const std::wstring& wName : splitDoubleNul(nameBlock)) {
    IniSection sec;
    sec.name = toUtf8(wName);

    // Whole section as raw "key=value" lines. This keeps the on-disk order AND
    // duplicate keys (ARK arrays repeat the same key), which a per-key
    // GetPrivateProfileStringW read cannot see - that is why the configurator
    // UI works from this full dump.
    const std::wstring lineBlock = readProfileBlock([&wPath, &wName](wchar_t* buf, DWORD cap) {
      return GetPrivateProfileSectionW(wName.c_str(), buf, cap, wPath.c_str());
    });

    for (const std::wstring& wLine : splitDoubleNul(lineBlock)) {
      const std::string line = toUtf8(wLine);
      IniEntry e;
      const size_t eq = line.find('=');  // split on the FIRST '='
      if (eq == std::string::npos) {
        e.key = line;  // bare line -> key with empty value
      } else {
        e.key = line.substr(0, eq);
        e.value = line.substr(eq + 1);
      }
      sec.entries.push_back(std::move(e));
    }
    sections.push_back(std::move(sec));
  }
  return sections;
}

bool iniDeleteKey(const std::string& iniPath, const std::string& section, const std::string& key) {
  const std::wstring wPath = toWide(iniPath);
  const std::wstring wSection = toWide(section);
  const std::wstring wKey = toWide(key);
  // A NULL value deletes the key line entirely.
  return WritePrivateProfileStringW(wSection.c_str(), wKey.c_str(), nullptr, wPath.c_str()) != 0;
}

// --- raw key-family rewrite (ARK array settings) ------------------------------

namespace {

// ASCII case-insensitive equality (ini section/key names are plain ASCII).
bool equalsNoCase(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + ('a' - 'A'));
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + ('a' - 'A'));
    if (x != y) return false;
  }
  return true;
}

// One physical line: content without its terminator + the terminator itself
// ("\r\n", "\n", or "" for a last line without a trailing newline).
struct RawLine {
  std::string text;
  std::string eol;
};

std::vector<RawLine> splitRawLines(const std::string& text) {
  std::vector<RawLine> out;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t nl = text.find('\n', pos);
    if (nl == std::string::npos) {
      out.push_back({text.substr(pos), ""});
      break;
    }
    size_t end = nl;
    std::string eol = "\n";
    if (nl > pos && text[nl - 1] == '\r') {
      end = nl - 1;
      eol = "\r\n";
    }
    out.push_back({text.substr(pos, end - pos), eol});
    pos = nl + 1;
  }
  return out;
}

// "[Name]" (after trimming) -> true + the trimmed inner name.
bool sectionHeader(const std::string& line, std::string& name) {
  const std::string t = trim(line);
  if (t.size() < 2 || t.front() != '[' || t.back() != ']') return false;
  name = trim(t.substr(1, t.size() - 2));
  return true;
}

// The family rule: the key part (before the first '=', trimmed) equals `key`
// or starts with `key` immediately followed by "[" (both case-insensitive).
bool matchesKeyFamily(const std::string& line, const std::string& key) {
  const size_t eq = line.find('=');
  const std::string keyPart = trim(eq == std::string::npos ? line : line.substr(0, eq));
  if (equalsNoCase(keyPart, key)) return true;
  return keyPart.size() > key.size() && keyPart[key.size()] == '[' &&
         equalsNoCase(keyPart.substr(0, key.size()), key);
}

}  // namespace

std::string replaceKeyLinesInText(const std::string& text, const std::string& section,
                                  const std::string& key,
                                  const std::vector<std::string>& fullLines) {
  const std::string wantSection = trim(section);
  const std::string wantKey = trim(key);
  if (wantSection.empty() || wantKey.empty()) return text;

  const std::vector<RawLine> lines = splitRawLines(text);

  // Dominant line ending of the file; CRLF for empty / terminator-less input.
  size_t crlf = 0, lf = 0;
  for (const RawLine& l : lines) {
    if (l.eol == "\r\n") ++crlf;
    else if (l.eol == "\n") ++lf;
  }
  const std::string eol = (lf > crlf) ? "\n" : "\r\n";

  // Locate the FIRST [section] occurrence and its end (next header or EOF).
  const size_t kNone = std::string::npos;
  size_t secStart = kNone, secEnd = lines.size();
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string name;
    if (!sectionHeader(lines[i].text, name)) continue;
    if (secStart == kNone) {
      if (equalsNoCase(name, wantSection)) secStart = i;
    } else {
      secEnd = i;
      break;
    }
  }

  if (secStart == kNone) {
    // Section missing: nothing to remove; create the header at EOF on insert.
    if (fullLines.empty()) return text;
    std::string out = text;
    if (!out.empty() && out.back() != '\n') out += eol;
    out += "[" + wantSection + "]" + eol;
    for (const std::string& l : fullLines) out += l + eol;
    return out;
  }

  // Mark the family's lines within the section; remember the first one.
  std::vector<char> removeLine(lines.size(), 0);
  size_t firstMatch = kNone;
  for (size_t i = secStart + 1; i < secEnd; ++i) {
    if (matchesKeyFamily(lines[i].text, wantKey)) {
      removeLine[i] = 1;
      if (firstMatch == kNone) firstMatch = i;
    }
  }
  if (firstMatch == kNone && fullLines.empty()) return text;  // nothing to do

  const size_t insertAt = (firstMatch != kNone) ? firstMatch : secEnd;
  std::string out;
  out.reserve(text.size() + 64);
  for (size_t i = 0; i <= lines.size(); ++i) {
    if (i == insertAt && !fullLines.empty()) {
      // Appending at EOF: give the previous (terminator-less) line an EOL first.
      if (i == lines.size() && !out.empty() && out.back() != '\n') out += eol;
      for (const std::string& l : fullLines) out += l + eol;
    }
    if (i == lines.size()) break;
    if (!removeLine[i]) {
      out += lines[i].text;
      out += lines[i].eol;
    }
  }
  return out;
}

bool iniReplaceKeyLines(const std::string& iniPath, const std::string& section,
                        const std::string& key, const std::vector<std::string>& fullLines) {
  const std::wstring wPath = toWide(iniPath);

  // Read the file as raw bytes (missing file == empty text -> created below).
  std::string bytes;
  {
    std::ifstream f(wPath.c_str(), std::ios::binary);
    if (f)
      bytes.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  }

  // UTF-16LE BOM (FF FE): decode to UTF-8 for the core, re-encode on write.
  // Everything else is 8-bit passthrough (UTF-8/ANSI preserved byte-for-byte).
  const bool utf16 = bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
                     static_cast<unsigned char>(bytes[1]) == 0xFE;
  std::string textUtf8;
  if (utf16) {
    const size_t wchars = (bytes.size() - 2) / 2;
    std::wstring wide(wchars, L'\0');
    if (wchars) std::memcpy(wide.data(), bytes.data() + 2, wchars * 2);
    textUtf8 = toUtf8(wide);
  } else {
    textUtf8 = bytes;
  }

  const std::string result = replaceKeyLinesInText(textUtf8, section, key, fullLines);

  std::string outBytes;
  if (utf16) {
    const std::wstring wide = toWide(result);
    outBytes.assign("\xFF\xFE", 2);
    outBytes.append(reinterpret_cast<const char*>(wide.data()), wide.size() * 2);
  } else {
    outBytes = result;
  }

  // Atomic-ish swap: write <path>.tmp, then move it over the original.
  const std::string tmpPath = iniPath + ".tmp";
  {
    std::ofstream f(toWide(tmpPath).c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(outBytes.data(), static_cast<std::streamsize>(outBytes.size()));
    f.close();
    if (!f) return false;
  }
  return MoveFileExW(toWide(tmpPath).c_str(), wPath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

}  // namespace amucore
