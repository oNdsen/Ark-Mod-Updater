#include "amucore/workshop.h"

#include <cstddef>
#include <vector>

namespace amucore {

namespace {

constexpr size_t kNpos = std::string::npos;

// Text between `open` and the next `close`, starting the search at `from`.
// Returns "" (and leaves *matchEnd untouched) when the pair is not found.
std::string between(const std::string& s, const char* open, const char* close,
                    size_t from, size_t* matchEnd) {
  const size_t o = s.find(open, from);
  if (o == kNpos) return {};
  const size_t start = o + std::char_traits<char>::length(open);
  const size_t c = s.find(close, start);
  if (c == kNpos) return {};
  if (matchEnd) *matchEnd = c;
  return s.substr(start, c - start);
}

// FIRST `class="workshopItemTitle">...</div>` capture (the AutoIt code takes [0]).
// Empty string signals "no title" -> item unavailable.
std::string firstBetween(const std::string& s, const char* open, const char* close) {
  return between(s, open, close, 0, nullptr);
}

// LAST occurrence of a `class="..."` value block, matching the AutoIt use of
// UBound()-1 for detailsStatRight (it takes the last of the global matches).
std::string lastBetween(const std::string& s, const char* open, const char* close) {
  std::string last;
  size_t from = 0;
  for (;;) {
    const size_t o = s.find(open, from);
    if (o == kNpos) break;
    const size_t start = o + std::char_traits<char>::length(open);
    const size_t c = s.find(close, start);
    if (c == kNpos) break;
    last = s.substr(start, c - start);
    from = c + 1;
  }
  return last;
}

// LAST ShowEnlargedImagePreview(..'<url>' url. The AutoIt pattern is
// "ShowEnlargedImagePreview..'(.*?)' " : the literal call name, then two arbitrary
// chars, then a single quote, capture up to the next quote followed by a space.
std::string lastEnlargedPreview(const std::string& s) {
  static const char kCall[] = "ShowEnlargedImagePreview";
  std::string last;
  size_t from = 0;
  for (;;) {
    const size_t p = s.find(kCall, from);
    if (p == kNpos) break;
    size_t q = p + (sizeof(kCall) - 1);
    // Skip exactly the two arbitrary chars ("..") then require a single quote.
    if (q + 3 > s.size()) break;
    q += 2;
    if (s[q] != '\'') {
      from = p + 1;
      continue;
    }
    ++q;  // past the opening quote
    // Capture up to the next "' " (quote immediately followed by a space).
    size_t end = q;
    bool found = false;
    while (end + 1 < s.size()) {
      if (s[end] == '\'' && s[end + 1] == ' ') { found = true; break; }
      ++end;
    }
    if (found) last = s.substr(q, end - q);
    from = p + 1;
  }
  return last;
}

// Trim spaces and tabs from both ends (.acf indents its blocks with tabs).
std::string trimWs(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
  return s.substr(b, e - b);
}

// content="<url>" of the <meta property="og:image" ...> tag, if present.
std::string ogImage(const std::string& s) {
  const size_t tag = s.find("og:image");
  if (tag == kNpos) return {};
  size_t end = 0;
  return between(s, "content=\"", "\"", tag, &end);
}

// Every `open`...`close` text block, in file order.
std::vector<std::string> allBetween(const std::string& s, const char* open, const char* close) {
  std::vector<std::string> out;
  size_t from = 0;
  for (;;) {
    const size_t o = s.find(open, from);
    if (o == kNpos) break;
    const size_t start = o + std::char_traits<char>::length(open);
    const size_t c = s.find(close, start);
    if (c == kNpos) break;
    out.push_back(s.substr(start, c - start));
    from = c;
  }
  return out;
}

// Steam lays the stats block out as TWO COLUMNS: all detailsStatLeft labels
// (File Size / Posted / Updated) in one container, all detailsStatRight values
// in the next. A label therefore pairs with the value at the same INDEX - not
// with the next value in the file, which is always File Size's (the first
// version of this helper did exactly that and the fixture test caught it).
// "" when the label is absent or has no counterpart.
std::string statByLabel(const std::string& s, const char* label) {
  const std::vector<std::string> labels = allBetween(s, "class=\"detailsStatLeft\">", "</div>");
  const std::vector<std::string> values = allBetween(s, "class=\"detailsStatRight\">", "</div>");
  // The LIVE page renders the labels with a trailing space ("Posted </div>");
  // the 2018 fixture does not - so compare trimmed, and trim the value too.
  auto trimmed = [](const std::string& t) {
    size_t b = 0, e = t.size();
    while (b < e && (t[b] == ' ' || t[b] == '\t' || t[b] == '\r' || t[b] == '\n')) ++b;
    while (e > b && (t[e - 1] == ' ' || t[e - 1] == '\t' || t[e - 1] == '\r' || t[e - 1] == '\n')) --e;
    return t.substr(b, e - b);
  };
  for (size_t i = 0; i < labels.size() && i < values.size(); ++i)
    if (trimmed(labels[i]) == label) return trimmed(values[i]);
  return {};
}

}  // namespace

WorkshopModInfo parseWorkshopHtml(const std::string& html) {
  WorkshopModInfo info;

  const std::string title =
      firstBetween(html, "class=\"workshopItemTitle\">", "</div>");
  if (title.empty()) {
    // No item title -> Workshop item removed / private / not available.
    info.available = false;
    return info;
  }

  info.name = title;
  info.available = true;

  // Preview URL: prefer the last ShowEnlargedImagePreview url (what the app uses),
  // then fall back to the og:image meta tag.
  info.previewUrl = lastEnlargedPreview(html);
  if (info.previewUrl.empty()) info.previewUrl = ogImage(html);

  info.date = lastBetween(html, "class=\"detailsStatRight\">", "</div>");
  info.posted = statByLabel(html, "Posted");

  return info;
}

AcfModInfo parseAcfForMod(const std::string& acfText, const std::string& modId) {
  AcfModInfo out;
  if (modId.empty()) return out;

  // Split into lines (handle \n and \r\n), matching FileReadToArray semantics.
  std::vector<std::string> lines;
  {
    size_t start = 0;
    while (start <= acfText.size()) {
      size_t nl = acfText.find('\n', start);
      std::string line = (nl == kNpos) ? acfText.substr(start)
                                       : acfText.substr(start, nl - start);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
      if (nl == kNpos) break;
      start = nl + 1;
    }
  }

  // Find the block HEADER line - the one whose whole content is the quoted mod id -
  // then read the three following value lines positionally: line+2, line+3, line+4.
  // The .acf block is
  //   "<modid>" / { / "size" "..." / "timeupdated" "..." / "manifest" "..." / }
  // A bare substring search (the AutoIt StringInStr) also matches another mod's
  // 19-digit "manifest" or 10-digit "timeupdated" value, and the three values would
  // then be read from the wrong block - which makes the updater believe a mod is up
  // to date (or reinstall it) forever.
  const std::string header = "\"" + modId + "\"";
  size_t line = 0;
  bool found = false;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (trimWs(lines[i]) == header) { line = i; found = true; break; }
  }
  if (!found) return out;
  const size_t last = lines.size();  // one past valid index
  if (line + 4 >= last) return out;

  // AutoIt StringSplit(line, '"') element [4] is the value between the 2nd pair of
  // quotes, i.e. the 4th quoted token counting from 1: "key" <ws> "value" -> [4]=value.
  auto quotedValue = [](const std::string& l) -> std::string {
    // element [4] == text between the 3rd and 4th double-quote.
    int quotes = 0;
    size_t valStart = kNpos;
    for (size_t i = 0; i < l.size(); ++i) {
      if (l[i] == '"') {
        ++quotes;
        if (quotes == 3) valStart = i + 1;
        else if (quotes == 4) return l.substr(valStart, i - valStart);
      }
    }
    return {};
  };

  out.size = quotedValue(lines[line + 2]);
  out.timeUpdated = quotedValue(lines[line + 3]);
  out.manifest = quotedValue(lines[line + 4]);
  return out;
}

}  // namespace amucore

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>

namespace amucore {

WorkshopModInfo getModInfo(uint64_t modId) {
  WorkshopModInfo info;  // available=false by default -> "neterror" on any failure

  HINTERNET hSession = WinHttpOpen(
      L"Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.0.5) "
      L"Gecko/2008120122 Firefox/3.0.5",
      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return info;

  HINTERNET hConnect =
      WinHttpConnect(hSession, L"steamcommunity.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return info;
  }

  std::wstring path = L"/sharedfiles/filedetails/?id=" + std::to_wstring(modId);
  HINTERNET hRequest =
      WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                         WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return info;
  }

  std::string body;
  BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr);
  if (ok) {
    for (;;) {
      DWORD avail = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
      std::string chunk(avail, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(hRequest, chunk.data(), avail, &read) || read == 0) break;
      body.append(chunk.data(), read);
    }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  if (!ok) return info;  // network failure -> available stays false ("neterror")
  return parseWorkshopHtml(body);
}

}  // namespace amucore

#endif  // _WIN32
