// AMU - Sciter.JS shell. Hosts the HTML/CSS UI (ui/main.html) in a Sciter window
// and exposes amucore to the UI as Window.this.amu.* (SOM passport).

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "amucore/credstore.h"
#include "amucore/db.h"
#include "amucore/orchestrator.h"
#include "amucore/serverconfig.h"
#include "amucore/supervisor.h"
#include "amucore/updatecheck.h"
#include "amucore/version.h"
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

// The "^[0-9]+$" Workshop-id check from _AddModToServer, plus a 19-digit cap so
// the value always fits an unsigned 64-bit id (Steam ids are uint64).
bool isModIdDigits(const std::string& s) {
  if (s.empty() || s.size() > 19) return false;
  for (char c : s)
    if (c < '0' || c > '9') return false;
  return true;
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
  }

  ~AmuWindow() override { supervisor_.shutdown(); }

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

      std::string name, size = "-", updated = "-", preview;
      for (const auto& m : mods) {
        if (std::to_string(m.modid) == id) {
          name = m.name;
          if (m.size > 0) size = humanSize(m.size);
          if (!m.date.empty()) updated = m.date;
          preview = m.preview;
          break;
        }
      }
      if (name.empty()) name = "Mod " + id;

      if (i) out += ",";
      out += "{\"id\":" + jstr(id);
      out += ",\"name\":" + jstr(name);
      out += ",\"state\":\"" + std::string(state) + "\"";
      out += ",\"size\":" + jstr(size);
      out += ",\"updated\":" + jstr(updated);
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
    std::string sessionName, gamePort = "7777", queryPort = "27015";
    std::string rconIp = "127.0.0.1", rconPort;
    bool rconEnabled = false;
    if (findServer(serverId, srv)) {
      const std::string ini = iniPathFor(srv.path);
      sessionName = amucore::iniRead(ini, "SessionSettings", "SessionName", "");
      gamePort = amucore::iniRead(ini, "SessionSettings", "Port", "7777");
      queryPort = amucore::iniRead(ini, "SessionSettings", "QueryPort", "27015");
      if (gamePort.empty()) gamePort = "7777";
      if (queryPort.empty()) queryPort = "27015";
      const amucore::RconEndpoint ep = makeRconEndpoint(serverId);
      rconIp = ep.host;
      rconPort = amucore::iniRead(ini, "ServerSettings", "RCONPort", "");
      rconEnabled = ep.enabled;
    }
    out += ",\"sessionName\":" + jstr(sessionName);
    out += ",\"gamePort\":" + jstr(gamePort);
    out += ",\"queryPort\":" + jstr(queryPort);
    out += ",\"rconIp\":" + jstr(rconIp);
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
    return amucore::iniReplaceKeyLines(iniPathForFile(srv.path, file), section, key,
                                       fullLines);
  }

  // --- mod update orchestrator bindings ---------------------------------------

  // Kick off an update run on the orchestrator worker (port of the "Install &&
  // Upgrade" button -> _DownloadAndInstallMods). serverId -1 = all servers,
  // onlyModId "" = all active mods. Returns false while a run is in progress.
  bool startUpdate(int serverId, std::string onlyModId) {
    return orchestrator_.start(serverId, onlyModId);
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

    // Ensure a mods-table row: new rows get name/preview only when the fetch
    // succeeded (bare id row otherwise, like the AutoIt INSERT branches); an
    // existing row gets name/preview refreshed on a successful fetch.
    amucore::Mod row;
    bool haveRow = false;
    for (const auto& m : db_.mods())
      if (m.modid == idNum) { row = m; haveRow = true; break; }
    if (!haveRow) {
      row.modid = idNum;
      row.size = 0;
      if (info.available) { row.name = info.name; row.preview = info.previewUrl; }
      db_.upsertMod(row);
    } else if (info.available) {
      row.name = info.name;
      row.preview = info.previewUrl;
      db_.upsertMod(row);
    }

    // NOTE: getModInfo folds the AutoIt "unavailable" and "neterror" cases into
    // available=false, so the placeholder covers both.
    const std::string displayName =
        info.available ? info.name : "(Workshop item not available)";
    db_.addLog(serverId, "normal", "Added mod " + id + " (" + displayName +
                                       ") to server " + std::to_string(serverId) +
                                       " (ActiveMods).");
    return result(true, false, info.available ? info.name : "", info.available);
  }

  // Remove a mod from a server: ActiveMods entry + local files + DB cache row
  // (port of _RemoveSelectedMod; the confirmation dialog is the UI's job).
  bool removeMod(int serverId, std::string modId) {
    const std::string id = stripAllWs(modId);
    amucore::Server srv;
    if (id.empty() || !findServer(serverId, srv)) return false;

    // 1) rewrite ActiveMods without this id (writes the rebuilt list either way).
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
      SOM_FUNC(clearLogs),
      SOM_FUNC(getUpdateCheck),
      SOM_FUNC(checkAppUpdate),
      SOM_FUNC(installAppUpdate)
    )
  SOM_PASSPORT_END

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
