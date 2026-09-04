// AMU - Sciter.JS shell. Hosts the HTML/CSS UI (ui/main.html) in a Sciter window
// and exposes amucore to the UI as Window.this.amu.* (SOM passport).

#include <windows.h>
#include <shobjidl.h>  // IFileOpenDialog (folder picker)

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "amucore/credstore.h"
#include "amucore/db.h"
#include "amucore/orchestrator.h"
#include "amucore/schedule.h"
#include "amucore/serverconfig.h"
#include "amucore/supervisor.h"
#include "amucore/updatecheck.h"
#include "amucore/version.h"
#include "amucore/warnplan.h"
#include "amucore/workshop.h"
#include "sciter-x.h"
#include "sciter-x-window.hpp"

namespace {

// UTF-8 <-> UTF-16 (the amucore APIs are UTF-8; the Win32 file APIs are wide).
std::wstring toWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}
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

// Minimal JSON escaping for the small, controlled payloads handed to the UI.
std::string jsonEscape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c) & 0xFF);
          o += buf;
        } else {
          o += c;
        }
    }
  }
  return o;
}
std::string jstr(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }

// ASCII lowercase (ini values like "True"/"False" are plain ASCII).
std::string toLowerAscii(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
  return s;
}

// "ASA" -> ArkGame::ASA, everything else -> ASE (the DB default).
amucore::ArkGame gameFrom(const std::string& g) {
  return g == "ASA" ? amucore::ArkGame::ASA : amucore::ArkGame::ASE;
}

// The WindowsServer config dir of a server install (matches the AutoIt layout).
std::string configDirFor(const std::string& serverPath) {
  return serverPath + "\\ShooterGame\\Saved\\Config\\WindowsServer";
}

// GameUserSettings.ini path for a server install directory (matches the AutoIt).
std::string iniPathFor(const std::string& serverPath) {
  return configDirFor(serverPath) + "\\GameUserSettings.ini";
}

// Configurator file selector: "gus" -> GameUserSettings.ini, "game" -> Game.ini.
// Unknown kinds fall back to GameUserSettings.ini (the safe/common case).
std::string iniPathForFile(const std::string& serverPath, const std::string& fileKind) {
  if (fileKind == "game") return configDirFor(serverPath) + "\\Game.ini";
  return iniPathFor(serverPath);
}

DWORD attrs(const std::string& path) { return GetFileAttributesW(toWide(path).c_str()); }
bool dirExists(const std::string& path) {
  DWORD a = attrs(path);
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
bool fileExists(const std::string& path) {
  DWORD a = attrs(path);
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// Last-write time of a file as local "DD.MM.YYYY HH:MM" (the app's date style),
// "" when it is missing. This is how the mod list learns its LOCAL install /
// update time: AMU rewrites <id>.mod on every install, so the file's mtime is
// exactly that moment - no extra DB column, and it survives a lost amu.db.
std::string fileMtimeLocal(const std::string& path) {
  WIN32_FILE_ATTRIBUTE_DATA fa{};
  if (!GetFileAttributesExW(toWide(path).c_str(), GetFileExInfoStandard, &fa)) return {};
  SYSTEMTIME utc{};
  SYSTEMTIME st{};
  // SystemTimeToTzSpecificLocalTime applies the DST rule that was in force AT
  // that time; FileTimeToLocalFileTime applies today's bias and shows files
  // from the other half of the year one hour off.
  if (!FileTimeToSystemTime(&fa.ftLastWriteTime, &utc) ||
      !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &st)) {
    return {};
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02u.%02u.%04u %02u:%02u", st.wDay, st.wMonth, st.wYear,
                st.wHour, st.wMinute);
  return buf;
}

// bytes -> "245 MB" / "1.3 GB" / "-" for zero, matching the compact UI labels.
std::string humanSize(int64_t bytes) {
  if (bytes <= 0) return "-";
  const double kb = bytes / 1024.0, mb = kb / 1024.0, gb = mb / 1024.0;
  char buf[32];
  if (gb >= 1.0) std::snprintf(buf, sizeof(buf), "%.1f GB", gb);
  else if (mb >= 1.0) std::snprintf(buf, sizeof(buf), "%.0f MB", mb);
  else std::snprintf(buf, sizeof(buf), "%.0f KB", kb < 1.0 ? 1.0 : kb);
  return std::string(buf);
}

// The lib\amu.db next to the running exe (same layout as the AutoIt app).
std::string dbPathNextToExe() {
  wchar_t exe[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring dir(exe);
  size_t slash = dir.find_last_of(L"\\/");
  if (slash != std::wstring::npos) dir.resize(slash);
  return toUtf8(dir + L"\\lib\\amu.db");
}

// The lib\ folder next to the exe - the orchestrator finds steamcmd (and its
// workshop download cache) at lib\steamcmd\, same layout as the AutoIt app.
std::string libDirNextToExe() {
  wchar_t exe[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring dir(exe);
  size_t slash = dir.find_last_of(L"\\/");
  if (slash != std::wstring::npos) dir.resize(slash);
  return toUtf8(dir + L"\\lib");
}

// lib\cache\previews next to the exe. Workshop preview images are cached as
// FILES rather than handed to the UI as https URLs: the mod list then paints
// instantly and offline, and Steam's CDN is hit once per mod instead of on
// every repaint. Deleting the folder is a safe way to force a re-fetch.
std::string previewDirNextToExe() { return libDirNextToExe() + "\\cache\\previews"; }

// The four container formats Sciter decodes, in the order they are probed on
// disk. Sciter picks its decoder from the file NAME, but Steam preview URLs
// carry no extension ("/ugc/<id>/<hash>/?imw=268"), so the download sniffs the
// magic bytes and names the file accordingly.
const char* const kPreviewExts[] = {"jpg", "png", "gif", "webp"};

// Image type from the first bytes; "" for anything unrecognised (an HTML error
// page, a 404 body) so it is never cached as if it were a thumbnail.
const char* imageExtFromMagic(const std::string& head) {
  auto is = [&](const char* sig, size_t n) {
    return head.size() >= n && std::memcmp(head.data(), sig, n) == 0;
  };
  if (is("\xFF\xD8\xFF", 3)) return "jpg";
  if (is("\x89PNG\r\n\x1A\n", 8)) return "png";
  if (is("GIF8", 4)) return "gif";
  if (head.size() >= 12 && std::memcmp(head.data(), "RIFF", 4) == 0 &&
      std::memcmp(head.data() + 8, "WEBP", 4) == 0) {
    return "webp";
  }
  return "";
}

// The cached preview file for a mod id, or "" when nothing is on disk.
std::string previewFileFor(const std::string& modId) {
  for (const char* ext : kPreviewExts) {
    const std::string p = previewDirNextToExe() + "\\" + modId + "." + ext;
    if (fileExists(p)) return p;
  }
  return std::string();
}

// "C:\a b\c.png" -> "file:///C:/a%20b/c.png". Sciter loads the thumbnail from
// this URL, so everything outside the unreserved set is percent-encoded (the
// install dir may well be under "Program Files").
std::string fileUrl(const std::string& path) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out = "file:///";
  for (unsigned char c : path) {
    if (c == '\\' || c == '/') { out += '/'; continue; }
    const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                      c == '.' || c == '~' || c == ':';
    if (safe) { out += static_cast<char>(c); continue; }
    out += '%';
    out += kHex[c >> 4];
    out += kHex[c & 0x0F];
  }
  return out;
}

// The folder containing the running exe (the install dir the updater targets).
std::wstring exeDirWide() {
  wchar_t exe[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring dir(exe);
  size_t slash = dir.find_last_of(L"\\/");
  if (slash != std::wstring::npos) dir.resize(slash);
  return dir;
}

// Trim surrounding whitespace (version.txt usually ends with a newline).
std::string trimWs(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return std::string();
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

// StringStripWS($s, $STR_STRIPALL): remove ALL whitespace, incl. interior runs.
std::string stripAllWs(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s)
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') out += c;
  return out;
}

// Cap a UTF-8 string at `maxBytes` without cutting a multi-byte sequence in
// half (a stray lead byte would render as garbage in the UI and the RCON
// broadcast). Backs up over continuation bytes (10xxxxxx).
void truncateUtf8(std::string& s, size_t maxBytes) {
  if (s.size() <= maxBytes) return;
  size_t cut = maxBytes;
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
  s.resize(cut);
}

// Rolling backup of an ini file before AMU rewrites it: a timestamped copy in
// <config dir>\AMU-Backups\, the newest `keep` per file retained. One call per
// SAVE ACTION (not per key), so a Config-tab save with 40 changes leaves one
// backup. Best effort - a failed backup never blocks the write, but is logged
// by the caller through the return value.
bool backupIniFile(const std::string& iniPath, int keep = 10) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path src(toWide(iniPath));
  if (!fs::exists(src, ec)) return true;  // nothing to back up yet
  const fs::path dir = src.parent_path() / L"AMU-Backups";
  fs::create_directories(dir, ec);
  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t stamp[32];
  swprintf(stamp, 32, L"%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour,
           st.wMinute, st.wSecond);
  const std::wstring stem = src.stem().wstring();
  const fs::path dst = dir / (stem + L"." + stamp + src.extension().wstring());
  if (!fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec)) return false;
  // rotation: names sort chronologically because of the stamp
  std::vector<fs::path> mine;
  for (const auto& e : fs::directory_iterator(dir, ec)) {
    const std::wstring n = e.path().filename().wstring();
    if (n.rfind(stem + L".", 0) == 0 && e.path().extension() == src.extension()) mine.push_back(e.path());
  }
  std::sort(mine.begin(), mine.end());
  while (mine.size() > static_cast<size_t>(keep > 0 ? keep : 1)) {
    fs::remove(mine.front(), ec);
    mine.erase(mine.begin());
  }
  return true;
}

// The "^[0-9]+$" Workshop-id check from _AddModToServer, plus a 19-digit cap so
// the value always fits an unsigned 64-bit id (Steam ids are uint64).
bool isModIdDigits(const std::string& s) {
  if (s.empty() || s.size() > 19) return false;
  for (char c : s)
    if (c < '0' || c > '9') return false;
  return true;
}

// Download one Workshop preview image into `dir` as <modId>.<ext>. Runs on the
// metadata worker thread, so it deliberately takes everything by value and
// touches NOTHING owned by the window.
//
// Two-step on purpose: the body lands in a .part file, gets sniffed for a real
// image header, and is only then renamed into place. A CDN error page or a
// truncated transfer therefore never becomes a "cached thumbnail" that would
// suppress every later retry.
void cachePreviewImage(const std::string& dir, const std::string& modId,
                       const std::string& url) {
  if (!isModIdDigits(modId)) return;  // the id is part of the path we build
  const amucore::HttpsUrl u = amucore::splitHttpsUrl(url);
  if (!u.ok) return;  // scraped from a Workshop page - see splitHttpsUrl

  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string part = dir + "\\" + modId + ".part";
  const fs::path partPath(toWide(part));
  if (!amucore::httpDownloadFile(toWide(u.host), toWide(u.path), part)) {
    fs::remove(partPath, ec);
    return;
  }

  std::string head;
  {
    std::ifstream in(partPath, std::ios::binary);
    char buf[16] = {};
    in.read(buf, sizeof(buf));
    head.assign(buf, static_cast<size_t>(in.gcount()));
  }
  const char* ext = imageExtFromMagic(head);
  if (!ext || !*ext) {
    fs::remove(partPath, ec);
    return;
  }

  // Drop any stale copy under a different extension first, so previewFileFor()
  // can never find two files for one mod.
  for (const char* e : kPreviewExts)
    fs::remove(fs::path(toWide(dir + "\\" + modId + "." + e)), ec);
  fs::rename(partPath, fs::path(toWide(dir + "\\" + modId + "." + ext)), ec);
  if (ec) fs::remove(partPath, ec);
}

// Case-insensitive path equality (NTFS semantics; lstrcmpiW handles non-ASCII).
bool pathEqualsNoCase(const std::string& a, const std::string& b) {
  return lstrcmpiW(toWide(a).c_str(), toWide(b).c_str()) == 0;
}

// Drop trailing path separators ("D:\ARK\" == "D:\ARK"), keeping a drive root.
std::string trimTrailingSlashes(std::string p) {
  while (p.size() > 3 && (p.back() == '\\' || p.back() == '/')) p.pop_back();
  return p;
}

}  // namespace

class AmuWindow : public sciter::window {
 public:
  AmuWindow()
      : sciter::window(SW_MAIN | SW_ENABLE_DEBUG, RECT{100, 100, 1140, 800}),
        // Constructed after db_/supervisor_ (declaration order) - it only stores
        // the references here; its worker thread starts on startUpdate().
        orchestrator_(db_, supervisor_, amucore::OrchestratorConfig{libDirNextToExe()},
                      makeLogSink()) {
    db_.open(dbPathNextToExe());

    // Wire the supervisor. The providers read DB + GameUserSettings.ini live at
    // (re)start/stop time, so an ini edit is picked up on the next action. They
    // run on the watcher/worker threads - the SQLite amalgamation is built in
    // serialized mode, so sharing db_ across threads is safe. The log sink is
    // the same one the orchestrator uses: everything lands in the logs table.
    supervisor_.start(
        [this](int64_t serverId) { return makeLaunchCommand(serverId); },
        [this](int64_t serverId) { return makeRconEndpoint(serverId); },
        makeLogSink());
    for (const auto& s : db_.servers()) {
      const auto lc = db_.launch(s.id);
      supervisor_.track(s.id, s.path, s.map, gameFrom(lc.game), lc.autoRestart != 0);
    }
    // Auto-start pass AFTER everything is tracked: request a start for each
    // server flagged auto_start; the supervisor brings them up asynchronously.
    for (const auto& s : db_.servers()) {
      if (db_.launch(s.id).autoStart != 0) {
        supervisor_.requestStart(s.id);
        db_.addLog(s.id, "debug",
                   "Auto-start: requested start of server " + std::to_string(s.id));
      }
    }

    // First app-update check right at launch; the UI polls getUpdateCheck().
    startUpdateCheck();

    // Scheduled update checks (schedule.h): a 20s watcher over the schedules
    // table. Started last, so everything it touches (db_, orchestrator_) is up.
    schedThread_ = std::thread([this]() { schedulerLoop(); });
  }

  ~AmuWindow() override {
    schedStop_.store(true);
    if (schedThread_.joinable()) schedThread_.join();  // sleeps in 100ms slices
    supervisor_.shutdown();
  }

  // --- native functions exposed to the UI as Window.this.amu.* --------------

  std::string ping() { return std::string("pong from amucore ") + amucore::version(); }

  // All configured servers, each with its RCON fields (from GameUserSettings.ini)
  // and active-mod count. Returns a JSON array of objects.
  std::string getServers() {
    std::string out = "[";
    const auto servers = db_.servers();
    for (size_t i = 0; i < servers.size(); ++i) {
      const auto& s = servers[i];
      const std::string ini = iniPathFor(s.path);
      std::string rconIp = amucore::iniRead(ini, "SessionSettings", "MultiHome", "");
      if (rconIp == "0") rconIp = "";  // MultiHome 0 == "no IP set"
      const std::string rconPort = amucore::iniRead(ini, "ServerSettings", "RCONPort", "");
      const int modCount = static_cast<int>(amucore::readActiveMods(ini).size());
      const auto lc = db_.launch(s.id);
      if (i) out += ",";
      out += "{\"id\":" + std::to_string(s.id);
      out += ",\"name\":" + jstr(s.name);
      out += ",\"map\":" + jstr(s.map);
      out += ",\"path\":" + jstr(s.path);
      out += ",\"game\":" + jstr(lc.game);
      out += ",\"desiredState\":" + std::to_string(lc.desiredState);
      out += ",\"rconIp\":" + jstr(rconIp);
      out += ",\"rconPort\":" + jstr(rconPort);
      out += ",\"online\":false";  // live status arrives with the RCON-ping binding
      out += ",\"modCount\":" + std::to_string(modCount);
      out += "}";
    }
    out += "]";
    return out;
  }

  // The active mods of one server: ActiveMods (from the ini) joined with the
  // cached mods table, with an install status computed from the filesystem
  // exactly like _FillModList (folder + .mod => Installed; folder only =>
  // Incomplete; neither => Not installed). Returns a JSON array.
  std::string getServerMods(int serverId) {
    std::string path;
    for (const auto& s : db_.servers())
      if (s.id == serverId) { path = s.path; break; }
    std::string out = "[";
    if (path.empty()) return out + "]";

    const auto ids = amucore::readActiveMods(iniPathFor(path));
    const auto mods = db_.mods();
    for (size_t i = 0; i < ids.size(); ++i) {
      const std::string& id = ids[i];
      const std::string folder = path + "\\ShooterGame\\Content\\Mods\\" + id;
      const bool hasFolder = dirExists(folder);
      const bool installed = hasFolder && fileExists(folder + ".mod");
      const char* state = installed ? "ok" : (hasFolder ? "inc" : "no");

      std::string name, size = "-", updated = "-", released = "-";
      for (const auto& m : mods) {
        if (std::to_string(m.modid) == id) {
          name = m.name;
          if (m.size > 0) size = humanSize(m.size);
          if (!m.date.empty()) updated = m.date;      // Workshop "Updated"
          if (!m.posted.empty()) released = m.posted;  // Workshop "Posted"
          break;
        }
      }
      // Local install/update time = mtime of the .mod file AMU writes last.
      const std::string installedAt = installed ? fileMtimeLocal(folder + ".mod") : std::string();
      if (name.empty()) name = "Mod " + id;

      // The UI gets the CACHED FILE, not mods.preview (which holds the Workshop
      // URL the downloader used). Empty until refreshModMeta() has fetched it -
      // the UI then falls back to the grey placeholder box.
      const std::string cached = previewFileFor(id);
      const std::string preview = cached.empty() ? std::string() : fileUrl(cached);

      if (i) out += ",";
      out += "{\"id\":" + jstr(id);
      out += ",\"name\":" + jstr(name);
      out += ",\"state\":\"" + std::string(state) + "\"";
      out += ",\"size\":" + jstr(size);
      out += ",\"updated\":" + jstr(updated);
      out += ",\"released\":" + jstr(released);
      out += ",\"installedAt\":" + jstr(installedAt.empty() ? std::string("-") : installedAt);
      out += ",\"preview\":" + jstr(preview);
      out += "}";
    }
    out += "]";
    return out;
  }

  // The newest `limit` log entries (all servers), newest first. Returns a JSON array.
  std::string getLogs(int limit) {
    if (limit <= 0) limit = 200;
    std::string out = "[";
    const auto entries = db_.logs(limit);
    for (size_t i = 0; i < entries.size(); ++i) {
      const auto& e = entries[i];
      // Full "YYYY/MM/DD HH:MM:SS" stamp, exactly as _FillLog displayed it.
      // Only 'normal' is always visible; 'debug' AND 'steamcmd' are the verbose
      // tier the Debug toggle adds (mirrors _FillLog's type IN(...) filter).
      const bool dbg = (e.type != "normal");
      if (i) out += ",";
      out += "{\"t\":" + jstr(e.date);
      out += ",\"m\":" + jstr(e.entry);
      out += ",\"type\":" + jstr(e.type);
      out += ",\"dbg\":" + std::string(dbg ? "true" : "false");
      out += "}";
    }
    out += "]";
    return out;
  }

  // --- supervisor bindings ---------------------------------------------------

  // Live status for all tracked servers (fast mutex-guarded snapshot). JSON array.
  std::string getStatuses() {
    std::string out = "[";
    const auto all = supervisor_.statusAll();
    for (size_t i = 0; i < all.size(); ++i) {
      if (i) out += ",";
      out += statusJson(all[i]);
    }
    out += "]";
    return out;
  }

  // Live status of one server. JSON object.
  std::string getServerStatus(int serverId) { return statusJson(supervisor_.status(serverId)); }

  // Start/Stop/Restart: NON-BLOCKING - they only flip desired-state and wake the
  // watcher; the UI sees starting/stopping on the next getStatuses poll. Start
  // and Stop additionally persist the intent (launch.desired_state) so it
  // survives an app restart - best-effort, after the supervisor call.
  bool startServer(int serverId) {
    supervisor_.requestStart(serverId);
    persistDesiredState(serverId, 1);
    return true;
  }
  bool stopServer(int serverId) {
    supervisor_.requestStop(serverId);
    persistDesiredState(serverId, 0);
    return true;
  }
  bool restartServer(int serverId) {
    supervisor_.requestRestart(serverId);
    return true;
  }

  // Merged launch config for the UI: the DB-owned launch row plus the ini-owned
  // display fields read live (single source of truth: ports/session/RCON live in
  // GameUserSettings.ini only, never in the DB). JSON object.
  std::string getLaunchConfig(int serverId) {
    const auto lc = db_.launch(serverId);
    std::string out = "{\"game\":" + jstr(lc.game);
    out += ",\"maxPlayers\":" + std::to_string(lc.maxPlayers);
    out += ",\"extraArgs\":" + jstr(lc.extraArgs);
    out += ",\"autoRestart\":" + std::to_string(lc.autoRestart);
    out += ",\"autoStart\":" + std::to_string(lc.autoStart);
    out += ",\"battleye\":" + std::to_string(lc.battleye);
    out += ",\"crossplay\":" + std::to_string(lc.crossplay);
    out += ",\"autoManaged\":" + std::to_string(lc.autoManaged);
    out += ",\"clusterId\":" + jstr(lc.clusterId);
    out += ",\"clusterDir\":" + jstr(lc.clusterDir);
    out += ",\"perfFlags\":" + jstr(lc.perfFlags);
    out += ",\"extraFlags\":" + jstr(lc.extraFlags);

    amucore::Server srv;
    std::string sessionName, gamePort = "7777", queryPort = "27015", rconPort;
    bool rconEnabled = false;
    if (findServer(serverId, srv)) {
      const std::string ini = iniPathFor(srv.path);
      sessionName = amucore::iniRead(ini, "SessionSettings", "SessionName", "");
      gamePort = amucore::iniRead(ini, "SessionSettings", "Port", "7777");
      queryPort = amucore::iniRead(ini, "SessionSettings", "QueryPort", "27015");
      if (gamePort.empty()) gamePort = "7777";
      if (queryPort.empty()) queryPort = "27015";
      rconPort = amucore::iniRead(ini, "ServerSettings", "RCONPort", "");
      rconEnabled = makeRconEndpoint(serverId).enabled;
    }
    // No "rconIp" here on purpose: getServers() already emits it as the RAW
    // MultiHome value ("" when unset), which is what the UI shows. Emitting the
    // EFFECTIVE host (127.0.0.1 fallback) under the same key from a second
    // binding invited a silent "not set" -> "127.0.0.1" mix-up.
    out += ",\"sessionName\":" + jstr(sessionName);
    out += ",\"gamePort\":" + jstr(gamePort);
    out += ",\"queryPort\":" + jstr(queryPort);
    out += ",\"rconPort\":" + jstr(rconPort);
    out += ",\"rconEnabled\":" + std::string(rconEnabled ? "true" : "false");
    out += "}";
    return out;
  }

  // Persist a launch config edit: DB-owned fields -> launch table, ini-owned
  // fields (SessionName/Port/QueryPort) -> GameUserSettings.ini (only when a
  // non-empty value was provided), then re-track so game/autoRestart changes
  // take effect on the supervisor immediately.
  bool saveLaunchConfig(int serverId, sciter::value cfg) {
    cfg.isolate();  // JS object -> plain key/value map

    // Absent keys keep the current DB value (partial updates stay safe).
    amucore::LaunchConfig lc = db_.launch(serverId);
    lc.serverId = serverId;
    auto getStr = [&cfg](const char* key, const std::string& def) {
      const sciter::value v = cfg.get_item(key);
      if (v.is_undefined() || v.is_null()) return def;
      return toUtf8(v.to_string());
    };
    auto getInt = [&cfg](const char* key, int def) {
      const sciter::value v = cfg.get_item(key);
      if (v.is_undefined() || v.is_null()) return def;
      return v.get(def);
    };
    auto getFlag = [&cfg](const char* key, int def) -> int {
      const sciter::value v = cfg.get_item(key);
      if (v.is_undefined() || v.is_null()) return def;
      return v.get(def != 0) ? 1 : 0;
    };
    lc.game = getStr("game", lc.game) == "ASA" ? "ASA" : "ASE";
    lc.maxPlayers = getInt("maxPlayers", lc.maxPlayers);
    lc.extraArgs = getStr("extraArgs", lc.extraArgs);
    lc.autoRestart = getFlag("autoRestart", lc.autoRestart);
    lc.autoStart = getFlag("autoStart", lc.autoStart);
    lc.battleye = getFlag("battleye", lc.battleye);
    lc.crossplay = getFlag("crossplay", lc.crossplay);
    lc.autoManaged = getFlag("autoManaged", lc.autoManaged);
    lc.clusterId = getStr("clusterId", lc.clusterId);
    lc.clusterDir = getStr("clusterDir", lc.clusterDir);
    lc.perfFlags = getStr("perfFlags", lc.perfFlags);
    lc.extraFlags = getStr("extraFlags", lc.extraFlags);
    if (!db_.upsertLaunch(lc)) return false;

    amucore::Server srv;
    if (!findServer(serverId, srv)) return false;
    const std::string ini = iniPathFor(srv.path);
    // Ports arrive as JS numbers; an emptied field comes through as 0 - never
    // write a 0 port into the ini (skip instead, keeping the current value).
    auto isPort = [](const std::string& p) { return !p.empty() && p != "0"; };
    const std::string sessionName = getStr("sessionName", "");
    const std::string gamePort = getStr("gamePort", "");
    const std::string queryPort = getStr("queryPort", "");
    const std::string rconPort = getStr("rconPort", "");
    backupIniFile(ini);
    if (!sessionName.empty()) amucore::iniWrite(ini, "SessionSettings", "SessionName", sessionName);
    if (isPort(gamePort)) amucore::iniWrite(ini, "SessionSettings", "Port", gamePort);
    if (isPort(queryPort)) amucore::iniWrite(ini, "SessionSettings", "QueryPort", queryPort);
    if (isPort(rconPort)) amucore::iniWrite(ini, "ServerSettings", "RCONPort", rconPort);

    supervisor_.track(serverId, srv.path, srv.map, gameFrom(lc.game), lc.autoRestart != 0);
    return true;
  }

  // Update name/path/map of an existing server row.
  bool saveServer(int serverId, sciter::value obj) {
    obj.isolate();
    amucore::Server srv;
    if (!findServer(serverId, srv)) return false;
    auto getStr = [&obj](const char* key, const std::string& def) {
      const sciter::value v = obj.get_item(key);
      if (v.is_undefined() || v.is_null()) return def;
      return toUtf8(v.to_string());
    };
    srv.name = getStr("name", srv.name);
    srv.path = getStr("path", srv.path);
    srv.map = getStr("map", srv.map);
    if (db_.upsertServer(srv) < 0) return false;
    // Re-track so a changed path/map is picked up by the watcher.
    const auto lc = db_.launch(serverId);
    supervisor_.track(serverId, srv.path, srv.map, gameFrom(lc.game), lc.autoRestart != 0);
    return true;
  }

  // Remove a server: stop tracking it (running processes are left alone), then
  // delete the DB rows. Named ...ById because <windows.h> macro-claims DeleteServer.
  bool deleteServerById(int serverId) {
    supervisor_.untrack(serverId);
    db_.saveSchedules(serverId, {});  // its scheduled checks go with it
    return db_.deleteServer(serverId);
  }

  // Add a new server (port of _GetServerPath/_AddServer; the folder picker is
  // the UI's job). obj = {name,path,map,game}. Validates that the path holds a
  // server exe (ASE ShooterGameServer.exe or ASA ArkAscendedServer.exe) and is
  // not already configured; the settings row gets the AutoIt defaults (seeded
  // by upsertServer). Returns {"ok":bool,"id":int,"error":"..."}.
  std::string addServer(sciter::value obj) {
    obj.isolate();
    auto fail = [](const std::string& err) {
      return "{\"ok\":false,\"id\":0,\"error\":" + jstr(err) + "}";
    };
    const std::string name = itemStr(obj, "name");
    const std::string path = trimTrailingSlashes(itemStr(obj, "path"));
    if (name.empty() || path.empty()) return fail("invalid");

    // Already configured? (paths compare case-insensitively, like NTFS).
    for (const auto& s : db_.servers())
      if (pathEqualsNoCase(s.path, path)) return fail("duplicate");

    // The _check_srv_dir validity gate, extended with the ASA exe name.
    const std::string bin = path + "\\ShooterGame\\Binaries\\Win64\\";
    if (!fileExists(bin + "ShooterGameServer.exe") &&
        !fileExists(bin + "ArkAscendedServer.exe"))
      return fail("exe-not-found");

    amucore::Server sv;  // startscript stays empty (launch table owns startup)
    sv.name = name;
    sv.path = path;
    sv.map = itemStr(obj, "map");
    const int64_t id = db_.upsertServer(sv);  // seeds the AutoIt settings defaults
    if (id < 0) return fail(db_.lastError());

    amucore::LaunchConfig lc;
    lc.serverId = id;
    lc.game = itemStr(obj, "game") == "ASA" ? "ASA" : "ASE";
    db_.upsertLaunch(lc);
    supervisor_.track(id, sv.path, sv.map, gameFrom(lc.game), lc.autoRestart != 0);

    db_.addLog(id, "normal",
               "Added new Server with ID " + std::to_string(id) + ":" + name);
    return "{\"ok\":true,\"id\":" + std::to_string(id) + ",\"error\":\"\"}";
  }

  // --- global settings bindings ------------------------------------------------

  // The steamcmd login block of the "-1" settings row for the settings dialog.
  // The user name is decrypted for display; the password/guard NEVER leave the
  // native side - the UI only learns whether a value is stored.
  std::string getGlobalSettings() {
    const amucore::Settings g = db_.settings(-1);
    std::string out = "{\"anonymous\":";
    out += g.steamcmdAnonymous != 0 ? "true" : "false";
    out += ",\"user\":" + jstr(amucore::unprotect(g.steamcmdUser));
    out += ",\"passSet\":";
    out += !g.steamcmdPass.empty() ? "true" : "false";
    out += ",\"guardSet\":";
    out += !g.steamcmdGuard.empty() ? "true" : "false";
    out += "}";
    return out;
  }

  // Save the steamcmd login: obj = {anonymous,user,pass,guard}. Empty strings
  // mean "keep the stored value" (the UI saves without re-typing secrets);
  // non-empty values are DPAPI-encrypted first (parity with _CredEncrypt - the
  // user name is stored encrypted too). `anonymous` is always written.
  bool saveGlobalSettings(sciter::value obj) {
    obj.isolate();
    const sciter::value av = obj.get_item("anonymous");
    const int anonymous =
        (!av.is_undefined() && !av.is_null() && av.get(false)) ? 1 : 0;
    std::string user = itemStr(obj, "user");
    std::string pass = itemStr(obj, "pass");
    std::string guard = itemStr(obj, "guard");
    if (!user.empty()) user = amucore::protect(user);
    if (!pass.empty()) pass = amucore::protect(pass);
    if (!guard.empty()) guard = amucore::protect(guard);
    return db_.saveGlobalSteamcmd(anonymous, user, pass, guard);
  }

  // App + engine version for the About/settings pane. JSON object.
  std::string getAppInfo() {
    char sciterVer[32];
    std::snprintf(sciterVer, sizeof(sciterVer), "%u.%u.%u.%u", SciterVersion(0),
                  SciterVersion(1), SciterVersion(2), SciterVersion(3));
    return "{\"version\":" + jstr(amucore::version()) +
           ",\"sciter\":" + jstr(sciterVer) + "}";
  }

  // --- app self-update bindings ------------------------------------------------

  // Non-blocking snapshot of the app-update check for the UI poll. JSON object:
  // {"state":"idle|checking|uptodate|available|error","current","latest","error"}.
  std::string getUpdateCheck() {
    auto st = update_;
    std::lock_guard<std::mutex> lk(st->m);
    return "{\"state\":" + jstr(st->state) +
           ",\"current\":" + jstr(amucore::version()) +
           ",\"latest\":" + jstr(st->latest) +
           ",\"error\":" + jstr(st->error) + "}";
  }

  // Kick a re-check; the result lands in the snapshot (poll getUpdateCheck()).
  bool checkAppUpdate() {
    startUpdateCheck();
    return true;
  }

  // Launch the updater and close the app so it can swap the files. The updater
  // is COPIED to %TEMP% and started from there: that way NOTHING in the install
  // dir is locked during the swap - the manifest covers every file including
  // lib\amu_updater.exe itself, which would otherwise have to rename itself
  // around its own lock. Returns false when no update is available or the
  // updater exe is missing/failed to start.
  bool installAppUpdate() {
    {
      std::lock_guard<std::mutex> lk(update_->m);
      if (update_->state != "available") return false;
    }
    const std::wstring exeDir = exeDirWide();
    const std::wstring updater = exeDir + L"\\lib\\amu_updater.exe";
    if (!fileExists(toUtf8(updater))) return false;

    wchar_t tmpDir[MAX_PATH] = {};
    if (!GetTempPathW(MAX_PATH, tmpDir)) return false;
    const std::wstring tmpUpdater = std::wstring(tmpDir) + L"amu_updater_" +
                                    std::to_wstring(GetCurrentProcessId()) + L".exe";
    if (!CopyFileW(updater.c_str(), tmpUpdater.c_str(), FALSE)) return false;

    std::wstring cmd = L"\"" + tmpUpdater + L"\" --wait-pid " +
                       std::to_wstring(GetCurrentProcessId()) + L" --dir \"" +
                       exeDir + L"\"";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(tmpUpdater.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                        0, nullptr, tmpDir, &si, &pi))
      return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Queue the close - it is handled by the message loop AFTER this call has
    // returned to the UI, so the normal close path (supervisor shutdown etc.)
    // runs via the destructors as usual.
    ::PostMessageW(get_hwnd(), WM_CLOSE, 0, 0);
    return true;
  }

  // --- server configurator bindings ------------------------------------------
  // `file` is "gus" (GameUserSettings.ini) or "game" (Game.ini). The UI gets the
  // WHOLE ini as one dump because per-key reads cannot see duplicate keys (ARK
  // arrays repeat the same key) and would lose the on-disk order.

  // Full ini dump as JSON:
  // {"sections":[{"name":"...","entries":[["Key","Value"],...]},...]}
  // Unknown server or missing file -> {"sections":[]}.
  std::string getIniDump(int serverId, std::string file) {
    amucore::Server srv;
    std::string out = "{\"sections\":[";
    if (findServer(serverId, srv)) {
      const auto sections = amucore::iniReadAll(iniPathForFile(srv.path, file));
      for (size_t i = 0; i < sections.size(); ++i) {
        const auto& sec = sections[i];
        if (i) out += ",";
        out += "{\"name\":" + jstr(sec.name) + ",\"entries\":[";
        for (size_t j = 0; j < sec.entries.size(); ++j) {
          if (j) out += ",";
          out += "[" + jstr(sec.entries[j].key) + "," + jstr(sec.entries[j].value) + "]";
        }
        out += "]}";
      }
    }
    out += "]}";
    return out;
  }

  // Write a batch of values: edits = [{section,key,value},...]. Each write goes
  // through the profile API, so unrelated keys/sections are preserved. Returns
  // false when the server is unknown or any single write fails (writes are not
  // transactional - earlier edits stay applied).
  bool saveIniValues(int serverId, std::string file, sciter::value edits) {
    amucore::Server srv;
    if (!findServer(serverId, srv)) return false;
    const std::string ini = iniPathForFile(srv.path, file);
    backupIniFile(ini);  // one rolling backup per save action
    edits.isolate();
    bool ok = true;
    const int n = edits.length();
    for (int i = 0; i < n; ++i) {
      sciter::value item = edits.get_item(i);
      item.isolate();
      const std::string section = itemStr(item, "section");
      const std::string key = itemStr(item, "key");
      const std::string value = itemStr(item, "value");
      if (section.empty() || key.empty()) { ok = false; continue; }
      if (!amucore::iniWrite(ini, section, key, value)) ok = false;
    }
    return ok;
  }

  // Delete a batch of keys: dels = [{section,key},...]. Removing a key makes
  // ARK fall back to its built-in default for that setting.
  bool deleteIniKeys(int serverId, std::string file, sciter::value dels) {
    amucore::Server srv;
    if (!findServer(serverId, srv)) return false;
    const std::string ini = iniPathForFile(srv.path, file);
    backupIniFile(ini);
    dels.isolate();
    bool ok = true;
    const int n = dels.length();
    for (int i = 0; i < n; ++i) {
      sciter::value item = dels.get_item(i);
      item.isolate();
      const std::string section = itemStr(item, "section");
      const std::string key = itemStr(item, "key");
      if (section.empty() || key.empty()) { ok = false; continue; }
      if (!amucore::iniDeleteKey(ini, section, key)) ok = false;
    }
    return ok;
  }

  // Raw rewrite of an ARK array family inside one ini section. ARK's complex
  // Game.ini settings are ARRAYS - the SAME key repeated on many lines
  // (OverrideNamedEngramEntries=(...) x N) or an [i]-indexed family
  // (PerLevelStatsMultiplier_Player[0]=...) - and the profile API holds only
  // ONE value per key, so the UI sends the FULL raw lines (complete "Key=(...)"
  // / "Key[0]=..." strings, no newlines) and the section is rewritten as text.
  // Within [section] every line whose key equals `key` or starts with `key`
  // followed by "[" is replaced by `lines` at the first match's position (or
  // appended at the section end; the section is created when missing); an empty
  // array removes the family. Everything else (comments, other sections,
  // encoding) is preserved byte-for-byte.
  bool setIniKeyLines(int serverId, std::string file, std::string section,
                      std::string key, sciter::value lines) {
    if (section.empty() || key.empty()) return false;
    amucore::Server srv;
    if (!findServer(serverId, srv)) return false;
    lines.isolate();
    std::vector<std::string> fullLines;
    const int n = lines.length();
    for (int i = 0; i < n; ++i)
      fullLines.push_back(toUtf8(lines.get_item(i).to_string()));
    backupIniFile(iniPathForFile(srv.path, file));
    return amucore::iniReplaceKeyLines(iniPathForFile(srv.path, file), section, key,
                                       fullLines);
  }

  // --- mod update orchestrator bindings ---------------------------------------

  // Kick off an update run on the orchestrator worker (port of the "Install &&
  // Upgrade" button -> _DownloadAndInstallMods). serverId -1 = all servers,
  // onlyModId "" = all active mods. Returns false while a run is in progress.
  // onlyModIds: "" = every active mod, else a comma list (the Mods tab sends
  // the missing ones, or the selected one). force: reinstall even when the
  // manifest says up to date - the "Reinstall" button. Unlike the per-server
  // force flag from the AutoIt days it does NOT skip the RCON player warning.
  bool startUpdate(int serverId, std::string onlyModIds, bool force) {
    return orchestrator_.start(serverId, onlyModIds, force);
  }

  // Snapshot of the running (or last) update run for the UI poll. JSON object:
  // {"running":bool,"phase":"...","countdownSecs":int,"mods":[{"id","status"}],
  //  "lines":["...",...]} - lines newest LAST (capped by the orchestrator).
  std::string getUpdateStatus() {
    const amucore::UpdateStatus st = orchestrator_.status();
    std::string out = "{\"running\":";
    out += st.running ? "true" : "false";
    out += ",\"phase\":" + jstr(st.phase.empty() ? "idle" : st.phase);
    out += ",\"countdownSecs\":" + std::to_string(st.countdownSecs);
    out += ",\"mods\":[";
    for (size_t i = 0; i < st.mods.size(); ++i) {
      if (i) out += ",";
      out += "{\"id\":" + jstr(st.mods[i].modId);
      out += ",\"status\":" + jstr(st.mods[i].status) + "}";
    }
    out += "],\"lines\":[";
    for (size_t i = 0; i < st.lines.size(); ++i) {
      if (i) out += ",";
      out += jstr(st.lines[i]);
    }
    out += "]}";
    return out;
  }

  // "Skip Countdown": finish the current msg1/msg2 wait immediately (msg3 and
  // the graceful stop still run, matching the AutoIt cancel button).
  bool skipUpdateCountdown() {
    orchestrator_.skipCountdown();
    return true;
  }

  // Add a Workshop mod to a server's ActiveMods (port of _AddModToServer).
  // Returns {"ok":bool,"already":bool,"name":"...","available":bool}.
  std::string addMod(int serverId, std::string modId) {
    auto result = [](bool ok, bool already, const std::string& name, bool available) {
      std::string o = "{\"ok\":";
      o += ok ? "true" : "false";
      o += ",\"already\":";
      o += already ? "true" : "false";
      o += ",\"name\":" + jstr(name);
      o += ",\"available\":";
      o += available ? "true" : "false";
      o += "}";
      return o;
    };

    const std::string id = stripAllWs(modId);
    amucore::Server srv;
    if (!isModIdDigits(id) || !findServer(serverId, srv))
      return result(false, false, "", false);

    // Append to ActiveMods unless already present (the AutoIt dedup loop).
    backupIniFile(iniPathFor(srv.path));
    if (!amucore::addActiveMod(iniPathFor(srv.path), id)) {
      std::string name;  // already active: report the cached name if we have one
      for (const auto& m : db_.mods())
        if (std::to_string(m.modid) == id) { name = m.name; break; }
      return result(true, true, name, true);
    }

    // Resolve name/preview/availability from the Workshop detail page.
    const int64_t idNum =
        static_cast<int64_t>(std::strtoull(id.c_str(), nullptr, 10));
    const amucore::WorkshopModInfo info = amucore::getModInfo(static_cast<uint64_t>(idNum));

    // Ensure a mods-table row (bare id row like the AutoIt INSERT branches) and
    // refresh the metadata when the fetch succeeded. Targeted write: an update
    // run may own the install columns of this row right now.
    if (info.available)
      db_.updateModMeta(idNum, info.name, info.previewUrl, info.date, info.posted);
    else
      db_.updateModMeta(idNum, "", "", "", "");  // creates the row, keeps whatever is stored

    // NOTE: getModInfo folds the AutoIt "unavailable" and "neterror" cases into
    // available=false, so the placeholder covers both.
    const std::string displayName =
        info.available ? info.name : "(Workshop item not available)";
    db_.addLog(serverId, "normal", "Added mod " + id + " (" + displayName +
                                       ") to server " + std::to_string(serverId) +
                                       " (ActiveMods).");
    return result(true, false, info.available ? info.name : "", info.available);
  }

  // Mods tab "move up / move down": the ActiveMods= order IS ARK's load order
  // (left-most = highest priority). `csv` is the COMPLETE new list; anything
  // that is not a Workshop id is dropped, so the caller must send every id.
  bool setActiveMods(int serverId, std::string csv) {
    amucore::Server srv;
    if (!findServer(serverId, srv)) return false;
    std::vector<std::string> ids;
    for (const std::string& id : amucore::parseActiveMods(csv))
      if (isModIdDigits(id)) ids.push_back(id);
    const std::string ini = iniPathFor(srv.path);
    backupIniFile(ini);
    const bool ok = amucore::writeActiveMods(ini, ids);
    if (ok)
      db_.addLog(serverId, "normal", "ActiveMods order changed for server " +
                                         std::to_string(serverId) + ": " + amucore::joinActiveMods(ids));
    return ok;
  }

  // Remove a mod from a server: ActiveMods entry + local files + DB cache row
  // (port of _RemoveSelectedMod; the confirmation dialog is the UI's job).
  bool removeMod(int serverId, std::string modId) {
    const std::string id = stripAllWs(modId);
    amucore::Server srv;
    if (id.empty() || !findServer(serverId, srv)) return false;

    // 1) rewrite ActiveMods without this id (writes the rebuilt list either way).
    backupIniFile(iniPathFor(srv.path));
    amucore::removeActiveMod(iniPathFor(srv.path), id);

    // 2) delete the local mod files (if present): folder recursively + the .mod.
    namespace fs = std::filesystem;
    std::error_code ec;  // best-effort, like the AutoIt DirRemove/FileDelete
    const std::wstring base = toWide(srv.path + "\\ShooterGame\\Content\\Mods\\" + id);
    fs::remove_all(fs::path(base), ec);
    fs::remove(fs::path(base + L".mod"), ec);

    // 3) delete the DB cache row (Number($sModId) -> non-numeric ids delete nothing).
    db_.deleteMod(static_cast<int64_t>(std::strtoull(id.c_str(), nullptr, 10)));

    db_.addLog(serverId, "normal", "Removed mod " + id + " from server " +
                                       std::to_string(serverId) +
                                       " (ActiveMods + files + DB).");
    return true;
  }

  // Wipe the whole log (the AutoIt _ClearLog), then stamp the same marker entry
  // it wrote so the history records when the wipe happened.
  bool clearLogs() {
    if (!db_.clearLogs()) return false;
    db_.addLog(0, "normal", "Logfile Deleted.");
    return true;
  }

  // Native folder picker for the server Path field (typing an ARK install root
  // by hand is error-prone, and a wrong path only surfaces as "exe-not-found").
  // `start` pre-selects a folder when it still exists. Returns "" on cancel.
  std::string pickFolder(std::string title, std::string start) {
    std::string picked;
    // Sciter already runs an STA on this thread; RPC_E_CHANGED_MODE just means
    // COM was initialised differently, in which case we must NOT uninitialise.
    const HRESULT hrInit =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dlg)))) {
      DWORD opts = 0;
      if (SUCCEEDED(dlg->GetOptions(&opts)))
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
      if (!title.empty()) dlg->SetTitle(toWide(title).c_str());
      if (!start.empty() && dirExists(start)) {
        IShellItem* from = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(toWide(start).c_str(), nullptr,
                                                  IID_PPV_ARGS(&from)))) {
          dlg->SetFolder(from);
          from->Release();
        }
      }
      if (SUCCEEDED(dlg->Show(get_hwnd()))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
          PWSTR path = nullptr;
          if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
            picked = toUtf8(path);
            CoTaskMemFree(path);
          }
          item->Release();
        }
      }
      dlg->Release();
    }
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return picked;
  }

  SOM_PASSPORT_BEGIN_EX(amu, AmuWindow)
    SOM_FUNCS(
      SOM_FUNC(ping),
      SOM_FUNC(getServers),
      SOM_FUNC(getServerMods),
      SOM_FUNC(getLogs),
      SOM_FUNC(getStatuses),
      SOM_FUNC(getServerStatus),
      SOM_FUNC(startServer),
      SOM_FUNC(stopServer),
      SOM_FUNC(restartServer),
      SOM_FUNC(getLaunchConfig),
      SOM_FUNC(saveLaunchConfig),
      SOM_FUNC(saveServer),
      SOM_FUNC(deleteServerById),
      SOM_FUNC(pickFolder),
      SOM_FUNC(addServer),
      SOM_FUNC(getGlobalSettings),
      SOM_FUNC(saveGlobalSettings),
      SOM_FUNC(getAppInfo),
      SOM_FUNC(getIniDump),
      SOM_FUNC(saveIniValues),
      SOM_FUNC(deleteIniKeys),
      SOM_FUNC(setIniKeyLines),
      SOM_FUNC(startUpdate),
      SOM_FUNC(getUpdateStatus),
      SOM_FUNC(skipUpdateCountdown),
      SOM_FUNC(addMod),
      SOM_FUNC(removeMod),
      SOM_FUNC(setActiveMods),
      SOM_FUNC(refreshModMeta),
      SOM_FUNC(getModMetaStatus),
      SOM_FUNC(getWarnPlan),
      SOM_FUNC(saveWarnPlan),
      SOM_FUNC(getSchedules),
      SOM_FUNC(saveSchedules),
      SOM_FUNC(clearLogs),
      SOM_FUNC(getUpdateCheck),
      SOM_FUNC(checkAppUpdate),
      SOM_FUNC(installAppUpdate)
    )
  SOM_PASSPORT_END

 private:
  // --- Workshop metadata backfill --------------------------------------------
  // A mod that only ever appeared in ActiveMods= (hand-edited ini, or a server
  // adopted from the AutoIt app) has no cached name/date/preview, so the list
  // showed "Mod <id>", "-" and an empty thumbnail. refreshModMeta() scrapes the
  // gaps in the background; getModMetaStatus() writes the finished rows to the
  // DB on the UI thread and tells the UI when a repaint is worth it.
  struct ModMetaResult {
    int64_t modid = 0;
    std::string name;
    std::string date;    // Workshop "Updated"
    std::string posted;  // Workshop "Posted" - when the mod was first published
    std::string previewUrl;
  };
  // Same shared_ptr discipline as UpdateCheckState: the worker touches only
  // this struct and its own copies, NEVER `this` or db_ - a fetch in flight
  // when the window dies just finishes into a struct nobody reads any more.
  struct ModMetaState {
    std::mutex m;
    int total = 0;
    int done = 0;
    std::vector<ModMetaResult> pending;  // fetched, not yet written to the DB
    // Ids already looked up this session. A removed/private Workshop item
    // never yields data, so without this it would be scraped again on every
    // visit to the Mods tab.
    std::vector<std::string> tried;
    std::atomic<bool> running{false};
  };

 public:
  // Kick off one backfill pass for `serverId` (no-op while one is running).
  // Only mods actually missing something are queued, so revisiting the Mods tab
  // costs nothing once everything is cached.
  std::string refreshModMeta(int serverId) {
    std::shared_ptr<ModMetaState> st = modMeta_;
    if (st->running.exchange(true)) return getModMetaStatus();

    // Apply anything a previous pass left undrained (the UI stops polling when
    // you leave the Mods tab) BEFORE deciding what is still missing - otherwise
    // those mods would be scraped a second time.
    const int carried = drainModMeta();

    amucore::Server srv;
    std::vector<std::string> todo;
    if (findServer(serverId, srv)) {
      const auto mods = db_.mods();
      for (const std::string& id : amucore::readActiveMods(iniPathFor(srv.path))) {
        if (!isModIdDigits(id)) continue;
        const int64_t idNum =
            static_cast<int64_t>(std::strtoull(id.c_str(), nullptr, 10));
        bool haveName = false, havePosted = false;
        for (const auto& m : mods)
          if (m.modid == idNum) {
            haveName = !m.name.empty();
            havePosted = !m.posted.empty();  // "Released" - added in 2.0, so
            break;                           // older rows lack it
          }
        if (haveName && havePosted && !previewFileFor(id).empty()) continue;
        bool tried = false;
        {
          std::lock_guard<std::mutex> lk(st->m);
          for (const std::string& t : st->tried)
            if (t == id) { tried = true; break; }
        }
        if (!tried) todo.push_back(id);
      }
    }
    {
      std::lock_guard<std::mutex> lk(st->m);
      st->total = static_cast<int>(todo.size());
      st->done = 0;
      for (const std::string& id : todo) st->tried.push_back(id);
    }
    if (todo.empty()) {
      st->running.store(false);
      return modMetaJson(carried);
    }

    const std::string dir = previewDirNextToExe();
    std::thread([st, todo, dir]() {
      std::error_code ec;
      std::filesystem::create_directories(std::filesystem::path(toWide(dir)), ec);
      for (const std::string& id : todo) {
        const uint64_t idNum = std::strtoull(id.c_str(), nullptr, 10);
        const amucore::WorkshopModInfo info = amucore::getModInfo(idNum);
        if (info.available && !info.previewUrl.empty())
          cachePreviewImage(dir, id, info.previewUrl);
        {
          std::lock_guard<std::mutex> lk(st->m);
          // available=false covers both "item removed" and "network error",
          // and neither should overwrite a cached name. A TRANSPORT failure
          // (offline, Steam blip) must not burn the id's one try per session,
          // so it is taken off `tried` again and the next Mods-tab visit
          // retries; a definitive "unavailable" stays on the list.
          if (info.available) {
            st->pending.push_back(ModMetaResult{static_cast<int64_t>(idNum),
                                                info.name, info.date, info.posted,
                                                info.previewUrl});
          } else if (info.netError) {
            for (size_t k = 0; k < st->tried.size(); ++k)
              if (st->tried[k] == id) { st->tried.erase(st->tried.begin() + static_cast<long>(k)); break; }
          }
          st->done++;
        }
      }
      st->running.store(false);
    }).detach();
    return modMetaJson(carried);
  }

  // Drain + progress, the binding the UI polls. `applied` > 0 is its cue to
  // re-read the mod list and repaint.
  std::string getModMetaStatus() { return modMetaJson(drainModMeta()); }

  // --- pre-shutdown warnings (warnplan.h) -------------------------------------
  // The Server tab's list: [{minutes,text,enabled}]. A server that never stored
  // a plan shows the AutoIt msg1/msg2/msg3 columns as a plan, or the default.
  std::string getWarnPlan(int serverId) {
    const amucore::Settings st = db_.settings(serverId);
    std::vector<amucore::WarnStep> plan = amucore::parseWarnPlan(st.warnplan);
    if (plan.empty() && st.warnplan.empty()) {
      plan = amucore::legacyWarnPlan(st.restarttime, st.msg1, st.msg2, st.msg3);
      if (plan.empty()) plan = amucore::defaultWarnPlan();
    }
    std::string out = "[";
    for (size_t i = 0; i < plan.size(); ++i) {
      if (i) out += ",";
      out += "{\"minutes\":" + std::to_string(plan[i].minutes);
      out += ",\"enabled\":" + std::string(plan[i].enabled ? "true" : "false");
      out += ",\"text\":" + jstr(plan[i].text) + "}";
    }
    return out + "]";
  }

  // Stores the list exactly as given. An empty list is a valid choice ("stop
  // without any warning") and is stored as such - see formatWarnPlan.
  bool saveWarnPlan(int serverId, sciter::value arr) {
    arr.isolate();
    std::vector<amucore::WarnStep> plan;
    const int n = arr.is_array() ? arr.length() : 0;
    for (int i = 0; i < n; ++i) {
      const sciter::value it = arr.get_item(i);
      amucore::WarnStep st;
      st.minutes = itemInt(it, "minutes", 0);
      st.text = itemStr(it, "text");
      st.enabled = itemBool(it, "enabled", true);
      if (st.minutes < 0 || st.minutes > 24 * 60) continue;  // a day is the sane cap
      truncateUtf8(st.text, 300);
      plan.push_back(std::move(st));
    }
    return db_.saveWarnPlan(serverId, amucore::formatWarnPlan(plan));
  }

  // --- scheduled update checks (schedule.h), per server ----------------------
  std::string getSchedules(int serverId) {
    std::string out = "[";
    std::vector<amucore::Schedule> list;
    for (const amucore::Schedule& s : db_.schedules())
      if (s.serverId == serverId) list.push_back(s);
    for (size_t i = 0; i < list.size(); ++i) {
      const amucore::Schedule& s = list[i];
      if (i) out += ",";
      out += "{\"id\":" + std::to_string(s.id);
      out += ",\"enabled\":" + std::string(s.enabled ? "true" : "false");
      out += ",\"name\":" + jstr(s.name);
      out += ",\"hour\":" + std::to_string(s.hour);
      out += ",\"minute\":" + std::to_string(s.minute);
      out += ",\"days\":" + std::to_string(s.days);
      out += ",\"serverId\":" + std::to_string(s.serverId);
      out += ",\"lastRun\":" + jstr(s.lastRun) + "}";
    }
    return out + "]";
  }

  // Replaces this server's list; entries failing scheduleValid are dropped (the
  // UI clamps first, so that only guards hand-crafted input).
  bool saveSchedules(int serverId, sciter::value arr) {
    arr.isolate();
    std::vector<amucore::Schedule> list;
    const int n = arr.is_array() ? arr.length() : 0;
    for (int i = 0; i < n; ++i) {
      const sciter::value it = arr.get_item(i);
      amucore::Schedule s;
      s.id = itemInt(it, "id", 0);
      s.enabled = itemBool(it, "enabled", true);
      s.name = itemStr(it, "name");
      s.hour = itemInt(it, "hour", 4);
      s.minute = itemInt(it, "minute", 0);
      s.days = itemInt(it, "days", amucore::kAllDays) & amucore::kAllDays;
      s.serverId = serverId;
      // lastRun is deliberately NOT taken from the UI - Db::saveSchedules keeps
      // the stored stamp per id (the watcher owns it).
      truncateUtf8(s.name, 60);
      if (s.name.empty()) s.name = "Update check";
      if (!amucore::scheduleValid(s)) continue;
      list.push_back(std::move(s));
    }
    return db_.saveSchedules(serverId, list);
  }

 private:
  // The scheduler watcher: every 20s (100ms slices so shutdown is instant)
  // compare every schedule with the local clock. Minute precision plus the
  // last_run stamp guarantee exactly one firing per due minute. Runs off the UI
  // thread: db_ is serialized-mode SQLite and Orchestrator::start is an atomic
  // handshake, and the log lines land in the Logs tab like everything else.
  void schedulerLoop() {
    while (!schedStop_.load()) {
      for (int i = 0; i < 200 && !schedStop_.load(); ++i) Sleep(100);
      if (schedStop_.load()) break;
      try {
        schedulerTick();
      } catch (const std::exception& ex) {
        db_.addLog(-1, "normal", std::string("Scheduler: unexpected error - ") + ex.what());
      } catch (...) {
        db_.addLog(-1, "normal", "Scheduler: unexpected error");
      }
    }
  }

  // One evaluation of every schedule against the local clock. A due schedule
  // is stamped (so a minute never fires twice) and QUEUED; the queue is
  // drained one run at a time as soon as the orchestrator is free. Without the
  // queue, two servers scheduled for the same minute would leave the second
  // one skipped forever - the common "all servers at 04:00" setup.
  void schedulerTick() {
    const std::time_t now = std::time(nullptr);
    std::tm tmv{};
    localtime_s(&tmv, &now);
    const std::string stamp = amucore::minuteStamp(tmv.tm_year + 1900, tmv.tm_mon + 1,
                                                   tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
    const int wday = amucore::mondayIndex(tmv.tm_wday);
    for (const amucore::Schedule& s : db_.schedules()) {
      if (!amucore::scheduleDue(s, wday, tmv.tm_hour, tmv.tm_min, stamp)) continue;
      db_.markScheduleRun(s.id, stamp);
      schedQueue_.push_back(s);
      // no quotes around the name: Db::addLog strips quoted runs (AutoIt mirror)
      db_.addLog(s.serverId, "normal",
                 "Schedule " + s.name + " (id " + std::to_string(s.id) + ") is due - update check queued.");
    }
    if (schedQueue_.empty() || orchestrator_.running()) return;
    const amucore::Schedule s = schedQueue_.front();
    schedQueue_.pop_front();
    const std::string who = "Schedule " + s.name + " (id " + std::to_string(s.id) + ")";
    if (orchestrator_.start(s.serverId, "", false))
      db_.addLog(s.serverId, "normal", who + ": update check started for server " + std::to_string(s.serverId) + ".");
    else
      db_.addLog(s.serverId, "normal", who + ": could not start the update run.");
  }

  std::thread schedThread_;
  std::atomic<bool> schedStop_{false};
  std::deque<amucore::Schedule> schedQueue_;  // touched by the scheduler thread only

 public:

 private:
  // Write everything the worker has finished to the DB and return how many rows
  // landed. Runs on the UI thread, which is the one that owns db_.
  int drainModMeta() {
    std::vector<ModMetaResult> take;
    {
      std::lock_guard<std::mutex> lk(modMeta_->m);
      take.swap(modMeta_->pending);
    }
    if (take.empty()) return 0;

    int applied = 0;
    // Targeted write (updateModMeta creates the row if needed and keeps every
    // column we do not carry): an update run may be writing size/timeupdated
    // to the same row at this very moment, and a full-row replace from our
    // snapshot would revert it.
    for (const ModMetaResult& r : take)
      if (db_.updateModMeta(r.modid, r.name, r.previewUrl, r.date, r.posted)) ++applied;
    return applied;
  }

  std::string modMetaJson(int applied) {
    int total = 0;
    int done = 0;
    const bool running = modMeta_->running.load();
    {
      std::lock_guard<std::mutex> lk(modMeta_->m);
      total = modMeta_->total;
      done = modMeta_->done;
    }
    return "{\"running\":" + std::string(running ? "true" : "false") +
           ",\"total\":" + std::to_string(total) +
           ",\"done\":" + std::to_string(done) +
           ",\"applied\":" + std::to_string(applied) + "}";
  }

 public:

 private:
  // Mutex-guarded app-update snapshot. Held by shared_ptr so the detached check
  // thread only ever touches this struct (never `this`) - teardown-safe even if
  // the window dies while a check is in flight.
  struct UpdateCheckState {
    std::mutex m;
    std::string state = "idle";  // idle|checking|uptodate|available|error
    std::string latest;
    std::string error;
    std::atomic<bool> checking{false};
  };

  // Start one background version check (no-op while one is already running).
  // The thread copies the shared_ptr, does the blocking HTTP GET off the UI
  // thread, then re-locks to publish the result.
  void startUpdateCheck() {
    std::shared_ptr<UpdateCheckState> st = update_;
    if (st->checking.exchange(true)) return;  // a check is already in flight
    {
      std::lock_guard<std::mutex> lk(st->m);
      st->state = "checking";
      st->error.clear();
    }
    std::thread([st]() {
      const std::string body = amucore::httpGetText(
          amucore::kUpdateHost, std::wstring(amucore::kUpdateBasePath) + L"version.txt");
      const std::string latest = trimWs(body);
      {
        std::lock_guard<std::mutex> lk(st->m);
        if (latest.empty()) {
          st->state = "error";
          st->error = "update check failed (offline?)";
        } else {
          st->latest = latest;
          st->state = amucore::compareVersions(latest, amucore::version()) > 0
                          ? "available"
                          : "uptodate";
          st->error.clear();
        }
      }
      st->checking.store(false);
    }).detach();
  }

  // String field of an isolated JS object; "" for absent/null fields.
  static std::string itemStr(const sciter::value& item, const char* key) {
    const sciter::value v = item.get_item(key);
    if (v.is_undefined() || v.is_null()) return std::string();
    return toUtf8(v.to_string());
  }
  // Integer field; numbers as-is, numeric strings parsed, anything else `def`.
  static int itemInt(const sciter::value& item, const char* key, int def) {
    const sciter::value v = item.get_item(key);
    if (v.is_undefined() || v.is_null()) return def;
    if (v.is_string()) {
      const std::string s = toUtf8(v.to_string());
      char* end = nullptr;
      const long n = std::strtol(s.c_str(), &end, 10);
      return (end && end != s.c_str()) ? static_cast<int>(n) : def;
    }
    return v.get(def);
  }
  // Boolean field; JS booleans, 0/1 and "true"/"false" strings, else `def`.
  static bool itemBool(const sciter::value& item, const char* key, bool def) {
    const sciter::value v = item.get_item(key);
    if (v.is_undefined() || v.is_null()) return def;
    if (v.is_bool()) return v.get(def);
    if (v.is_int()) return v.get(0) != 0;
    if (v.is_string()) {
      const std::string s = toLowerAscii(toUtf8(v.to_string()));
      return s == "true" || s == "1";
    }
    return def;
  }

  // Persist the start/stop intent in launch.desired_state. Best-effort: called
  // AFTER the (non-blocking) supervisor request, so a DB hiccup never delays or
  // blocks the actual start/stop.
  void persistDesiredState(int64_t serverId, int desired) {
    amucore::LaunchConfig lc = db_.launch(serverId);
    lc.serverId = serverId;
    lc.desiredState = desired;
    db_.upsertLaunch(lc);
  }

  // The servers row for `serverId`; false when it does not exist.
  bool findServer(int64_t serverId, amucore::Server& out) {
    for (const auto& s : db_.servers())
      if (s.id == serverId) {
        out = s;
        return true;
      }
    return false;
  }

  // LaunchProvider: assemble the LaunchCommand from the DB launch row plus the
  // live GameUserSettings.ini (ports/RCON/session/mods are ini-owned). Called on
  // a supervisor worker thread at (re)start time.
  amucore::LaunchCommand makeLaunchCommand(int64_t serverId) {
    amucore::Server srv;
    findServer(serverId, srv);  // unknown id -> empty input -> start fails cleanly
    const auto lc = db_.launch(serverId);
    const std::string ini = iniPathFor(srv.path);

    amucore::LaunchInput in;
    in.game = gameFrom(lc.game);
    in.installPath = srv.path;
    in.map = srv.map;
    in.sessionName = amucore::iniRead(ini, "SessionSettings", "SessionName", "");
    in.port = amucore::iniRead(ini, "SessionSettings", "Port", "");
    in.queryPort = amucore::iniRead(ini, "SessionSettings", "QueryPort", "");
    in.rconEnabled =
        toLowerAscii(amucore::iniRead(ini, "ServerSettings", "RCONEnabled", "False")) == "true";
    in.rconPort = amucore::iniRead(ini, "ServerSettings", "RCONPort", "");
    in.multiHome = amucore::iniRead(ini, "SessionSettings", "MultiHome", "");
    in.activeMods = amucore::joinActiveMods(amucore::readActiveMods(ini));
    in.maxPlayers = lc.maxPlayers;
    in.battleye = lc.battleye != 0;
    in.crossplay = lc.crossplay != 0;
    in.autoManaged = lc.autoManaged != 0;
    // -automanagedmods makes the SERVER download/install mods itself and reads
    // the ids from Game.ini [ModInstaller] ModIDS= (one line each; the wiki:
    // "Mod IDs are listed in Game.ini under [ModInstaller]") - i.e. a second
    // copy of the list the Mods tab keeps in ActiveMods=. Mirror it here, at
    // the moment the server is about to read it, so the two can never diverge;
    // the Config tab deliberately has no ModIDS editor for the same reason.
    // With the flag off ModIDS is inert and Game.ini stays untouched.
    if (in.autoManaged && in.game == amucore::ArkGame::ASE) {
      std::vector<std::string> lines;
      for (const std::string& id : amucore::readActiveMods(ini)) lines.push_back("ModIDS=" + id);
      backupIniFile(iniPathForFile(srv.path, "game"));
      amucore::iniReplaceKeyLines(iniPathForFile(srv.path, "game"), "ModInstaller", "ModIDS",
                                  lines);
    }
    in.clusterId = lc.clusterId;
    in.clusterDir = lc.clusterDir;
    in.perfFlags = lc.perfFlags;
    in.extraFlags = lc.extraFlags;
    in.extraArgs = lc.extraArgs;
    return amucore::buildLaunch(in);
  }

  // RconProvider: endpoint for the graceful-stop sequence, read live from the ini.
  amucore::RconEndpoint makeRconEndpoint(int64_t serverId) {
    amucore::Server srv;
    amucore::RconEndpoint ep;
    if (!findServer(serverId, srv)) return ep;
    const std::string ini = iniPathFor(srv.path);
    ep.host = amucore::iniRead(ini, "SessionSettings", "MultiHome", "");
    if (ep.host.empty() || ep.host == "0") ep.host = "127.0.0.1";  // MultiHome 0 == unset
    ep.port = static_cast<uint16_t>(
        std::atoi(amucore::iniRead(ini, "ServerSettings", "RCONPort", "0").c_str()));
    ep.password = amucore::iniRead(ini, "ServerSettings", "ServerAdminPassword", "");
    ep.enabled =
        toLowerAscii(amucore::iniRead(ini, "ServerSettings", "RCONEnabled", "False")) == "true";
    return ep;
  }

  // One ServerStatus as a JSON object (shared by getStatuses/getServerStatus).
  static std::string statusJson(const amucore::ServerStatus& st) {
    std::string o = "{\"id\":" + std::to_string(st.serverId);
    o += ",\"state\":" + jstr(st.state);
    o += ",\"desired\":" + jstr(st.desired);
    o += ",\"pid\":" + std::to_string(st.pid);
    o += ",\"failures\":" + std::to_string(st.failures);
    o += ",\"detail\":" + jstr(st.detail);
    o += "}";
    return o;
  }

  // The shared log sink for supervisor + orchestrator (same lambda body the
  // AutoIt _write_log served): every lifecycle/update event -> logs table.
  amucore::Supervisor::LogSink makeLogSink() {
    return [this](int64_t serverId, const std::string& type, const std::string& msg) {
      db_.addLog(serverId, type, msg);
    };
  }

  // Declaration order is load-bearing: orchestrator_ takes db_ + supervisor_ by
  // reference in its ctor and must be destroyed first (it joins its worker).
  amucore::Db db_;
  amucore::Supervisor supervisor_;
  amucore::Orchestrator orchestrator_;
  std::shared_ptr<UpdateCheckState> update_ = std::make_shared<UpdateCheckState>();
  std::shared_ptr<ModMetaState> modMeta_ = std::make_shared<ModMetaState>();
};

// Build a file:// URL for a path next to the exe.
static std::wstring urlNextToExe(const wchar_t* rel) {
  wchar_t exe[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring dir(exe);
  const size_t slash = dir.find_last_of(L"\\/");
  if (slash != std::wstring::npos) dir.resize(slash);
  std::wstring path = dir + L"\\" + rel;
  for (auto& c : path)
    if (c == L'\\') c = L'/';
  return L"file://" + path;
}

// The ui/ folder packed into the exe by packfolder at build time (defines
// `const unsigned char resources[]`). Served via this://app/ URLs below.
#include "ui_resources.cpp"

int uimain(std::function<int()> run) {
  // A loose ui\main.html next to the exe wins (dev mode: HTML edits need no
  // recompile); end-user installs carry no ui folder and run the packed copy.
  sciter::archive::instance().open(aux::elements_of(resources));
  const bool looseUi = fileExists(toUtf8(exeDirWide() + L"\\ui\\main.html"));
  const std::wstring url =
      looseUi ? urlNextToExe(L"ui\\main.html") : std::wstring(L"this://app/main.html");

  sciter::om::hasset<AmuWindow> pwin = new AmuWindow();
  pwin->load(url.c_str());

  // The exe's icon resource covers Explorer and the taskbar, but the title-bar
  // icon comes from the WINDOW - Sciter registers its window class without one,
  // so it has to be set explicitly or Windows draws its generic default.
  if (HWND hwnd = pwin->get_hwnd()) {
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    if (HICON big = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                  GetSystemMetrics(SM_CXICON),
                                                  GetSystemMetrics(SM_CYICON), 0)))
      SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    if (HICON small_ = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                     GetSystemMetrics(SM_CXSMICON),
                                                     GetSystemMetrics(SM_CYSMICON), 0)))
      SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_));
  }

  pwin->expand();
  return run();
}

// GUI-subsystem entry (the SDK's sciter-main.cpp only ships console entries;
// it is compiled with SKIP_MAIN and provides application::hinstance/argv).
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  static const WCHAR* argvStub[] = {L"amu"};
  sciter::application::start(1, argvStub);
  const int r = uimain([]() -> int { return sciter::application::run(); });
  sciter::application::shutdown();
  return r;
}
