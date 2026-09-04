#include "amucore/backup.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdlib>
#include <exception>
#include <filesystem>

#include "amucore/rcon.h"
#include "amucore/schedule.h"
#include "amucore/serverconfig.h"
#include "amucore/warnplan.h"
#include "amucore/worldbackup.h"

namespace fs = std::filesystem;

namespace amucore {

namespace {

constexpr size_t kMaxLines = 200;

std::wstring toWide(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
  return w;
}

std::string fromWide(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

std::string lowerAscii(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

std::string nowStamp() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  return minuteStamp(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
}

std::string humanSize(uint64_t b) {
  char buf[32];
  if (b >= 1024ull * 1024 * 1024)
    _snprintf_s(buf, _TRUNCATE, "%.2f GB", static_cast<double>(b) / (1024.0 * 1024 * 1024));
  else if (b >= 1024ull * 1024)
    _snprintf_s(buf, _TRUNCATE, "%.1f MB", static_cast<double>(b) / (1024.0 * 1024));
  else
    _snprintf_s(buf, _TRUNCATE, "%llu KB", static_cast<unsigned long long>((b + 1023) / 1024));
  return buf;
}

int64_t ticksOf(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return static_cast<int64_t>(u.QuadPart);
}

}  // namespace

std::string backupFolder(const Server& srv, const Settings& cfg) {
  if (!cfg.backupDir.empty()) return cfg.backupDir;
  return srv.path + "\\ShooterGame\\Saved\\Backups";
}

std::string worldFilePath(const std::string& installRoot, const std::string& map) {
  const std::string base = savedArksDir(installRoot);
  std::error_code ec;
  const std::string nested = base + "\\" + map + "\\" + map + ".ark";
  if (fs::exists(fs::path(toWide(nested)), ec)) return nested;
  return base + "\\" + map + ".ark";
}

BackupOutcome runBackup(Db& db, const Server& srv, const Settings& cfg,
                        const std::function<void(const std::string&)>& line,
                        const Supervisor::LogSink& log, const std::atomic<bool>* abort) {
  BackupOutcome out;
  auto say = [&](const std::string& t) {
    if (line) line(t);
  };
  auto fail = [&](const std::string& why) {
    out.ok = false;
    out.error = why;
    say("Error: " + why);
    if (log) log(srv.id, "normal", "Map backup failed for server " + std::to_string(srv.id) + ": " + why);
    return out;
  };

  if (srv.path.empty() || srv.map.empty()) return fail("server path or map is not set");
  const std::string src = savedArksDir(srv.path);
  std::error_code ec;
  if (!fs::is_directory(fs::path(toWide(src)), ec)) return fail("SavedArks folder not found: " + src);

  const std::string folder = backupFolder(srv, cfg);
  fs::create_directories(fs::path(toWide(folder)), ec);
  if (!fs::is_directory(fs::path(toWide(folder)), ec))
    return fail("could not create the backup folder " + folder);

  SYSTEMTIME st;
  GetLocalTime(&st);
  const std::string name =
      backupFileName(srv.name, srv.map, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  const std::string dest = folder + "\\" + name;
  say("Compressing " + src + " ...");
  const ZipResult z = zipDirectory(src, srv.map, dest, false, abort);
  if (!z.ok) return fail(z.error);
  out.ok = true;
  out.file = dest;
  out.files = z.files;
  out.bytesIn = z.bytesIn;
  out.bytesOut = z.bytesOut;
  say("Map backup written: " + name + " (" + std::to_string(z.files) + " files, " + humanSize(z.bytesIn) +
      " -> " + humanSize(z.bytesOut) + ")");
  if (log)
    log(srv.id, "normal",
        "Map backup written: " + name + " (" + std::to_string(z.files) + " files, " + humanSize(z.bytesOut) + ")");

  // rotation - only this server/map's archives, oldest first
  const int keep = cfg.backupKeep > 0 ? cfg.backupKeep : 10;
  std::vector<std::string> names;
  for (const auto& e : fs::directory_iterator(fs::path(toWide(folder)), ec)) {
    if (!e.is_regular_file(ec)) continue;
    names.push_back(fromWide(e.path().filename().wstring()));
  }
  for (const std::string& old : backupsToDelete(names, backupPrefix(srv.name, srv.map), keep)) {
    if (fs::remove(fs::path(toWide(folder + "\\" + old)), ec)) {
      ++out.deleted;
      say("Rotated out: " + old);
    } else {
      say("Could not delete the old backup " + old);
    }
  }
  if (out.deleted && log)
    log(srv.id, "debug", "Map backup rotation: removed " + std::to_string(out.deleted) + " archive(s), keeping " +
                             std::to_string(keep));

  if (!db.markBackupRun(srv.id, nowStamp()) && log)
    log(srv.id, "normal", "Could not store the last-backup time: " + db.lastError());
  return out;
}

// ---------------------------------------------------------------------------

Backuper::Backuper(Db& db, Supervisor& sup, LogSink log) : db_(db), sup_(sup), logSink_(std::move(log)) {
  st_.phase = "idle";
}

Backuper::~Backuper() { shutdown(); }

void Backuper::shutdown() {
  abort_.store(true);
  if (worker_.joinable()) worker_.join();
}

bool Backuper::start(int64_t serverId, bool warn) {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) return false;
  if (worker_.joinable()) worker_.join();  // the previous run has finished (running_ was false)
  {
    std::lock_guard<std::mutex> lk(mu_);
    st_ = BackupStatus{};
    st_.running = true;
    st_.phase = "start";
    st_.serverId = serverId;
  }
  skip_.store(false);
  worker_ = std::thread([this, serverId, warn]() { run(serverId, warn); });
  return true;
}

void Backuper::skipCountdown() { skip_.store(true); }

BackupStatus Backuper::status() {
  std::lock_guard<std::mutex> lk(mu_);
  return st_;
}

void Backuper::line(const std::string& text) {
  std::lock_guard<std::mutex> lk(mu_);
  st_.lines.push_back(text);
  if (st_.lines.size() > kMaxLines) st_.lines.erase(st_.lines.begin());
}

void Backuper::setPhase(const char* phase) {
  std::lock_guard<std::mutex> lk(mu_);
  st_.phase = phase;
}

bool Backuper::waitSecs(int secs, int extra) {
  constexpr int kSliceMs = 100;
  bool skipped = false;
  for (int remaining = secs; remaining > 0; --remaining) {
    if (abort_.load()) break;
    if (skip_.exchange(false)) {
      skipped = true;
      break;
    }
    {
      std::lock_guard<std::mutex> lk(mu_);
      st_.countdownSecs = remaining + extra;
    }
    for (int slept = 0; slept < 1000; slept += kSliceMs) {
      Sleep(kSliceMs);
      if (abort_.load() || skip_.load()) break;
    }
  }
  std::lock_guard<std::mutex> lk(mu_);
  st_.countdownSecs = -1;
  return !skipped && !abort_.load();
}

void Backuper::run(int64_t serverId, bool warn) {
  auto finish = [this](bool ok, const std::string& err, const std::string& file) {
    std::lock_guard<std::mutex> lk(mu_);
    st_.running = false;
    st_.phase = ok ? "done" : "error";
    st_.countdownSecs = -1;
    st_.lastError = err;
    if (ok) st_.lastFile = file;
  };
  auto log = [this, serverId](const char* type, const std::string& msg) {
    if (logSink_) logSink_(serverId, type, msg);
  };
  try {
    Server srv;
    bool found = false;
    for (const Server& s : db_.servers())
      if (s.id == serverId) {
        srv = s;
        found = true;
        break;
      }
    if (!found) {
      line("Error: server " + std::to_string(serverId) + " not found.");
      finish(false, "server not found", "");
      running_.store(false);
      return;
    }
    const Settings cfg = db_.settings(serverId);
    const bool isRunning = sup_.status(serverId).running;
    line("Map backup for " + srv.name + " (" + srv.map + ")" + (isRunning ? "" : " - server is not running"));

    if (isRunning) {
      const std::string gus = srv.path + "\\ShooterGame\\Saved\\Config\\WindowsServer\\GameUserSettings.ini";
      const bool rconEnabled = lowerAscii(iniRead(gus, "ServerSettings", "RCONEnabled", "False")) == "true";
      std::string rconIp = iniRead(gus, "SessionSettings", "MultiHome", "0");
      if (rconIp == "0" || rconIp.empty()) rconIp = "127.0.0.1";
      const uint16_t rconPort = static_cast<uint16_t>(
          std::strtoul(iniRead(gus, "ServerSettings", "RCONPort", "").c_str(), nullptr, 10));
      const std::string rconPass = iniRead(gus, "ServerSettings", "ServerAdminPassword", "");
      auto rcon = [&](const std::string& cmd, const char* what) -> bool {
        RconClient rc;
        std::string resp;
        const RconStatus rs = rc.runCommand(rconIp, rconPort, rconPass, cmd, resp);
        if (rs != RconStatus::Ok) {
          line(std::string("RCON ") + what + " failed (status " + std::to_string(static_cast<int>(rs)) +
               ") - check RCONPort/ServerAdminPassword.");
          log("normal", std::string("RCON ") + what + " failed during the map backup of server " +
                            std::to_string(serverId) + " (status " + std::to_string(static_cast<int>(rs)) + ").");
          return false;
        }
        return true;
      };

      if (!rconEnabled) {
        line("RCON is disabled - no player message and no saveworld; archiving the last autosave.");
      } else {
        if (warn) {
          // The plan: stored -> as is (empty = deliberately silent); not stored -> default.
          std::vector<WarnStep> plan = parseWarnPlan(cfg.backupplan);
          if (plan.empty() && cfg.backupplan.empty()) plan = defaultBackupPlan();
          const std::vector<WarnStep> steps = countdownSteps(plan);
          if (!steps.empty()) {
            setPhase("countdown");
            bool skipped = false;
            for (size_t i = 0; i < steps.size() && !abort_.load(); ++i) {
              const std::string text = renderWarnText(steps[i].text, steps[i].minutes);
              line("RCON message (" + std::to_string(steps[i].minutes) + " min): " + text);
              rcon("broadcast " + text, "broadcast");
              if (skipped) continue;
              const int wait = secondsUntilNext(steps, i);
              const int after = (i + 1 < steps.size()) ? steps[i + 1].minutes * 60 : 0;
              if (wait > 0 && !waitSecs(wait, after)) skipped = true;
            }
          }
        }
        if (abort_.load()) {
          finish(false, "aborted", "");
          running_.store(false);
          return;
        }
        setPhase("save");
        line("Sending saveworld ...");
        if (rcon("saveworld", "saveworld")) {
          // Wait for a fresh <map>.ark (same contract as the update stop).
          const std::wstring world = toWide(worldFilePath(srv.path, srv.map));
          int waitedMs = 0;
          bool fresh = false;
          while (!abort_.load()) {
            WIN32_FILE_ATTRIBUTE_DATA fad{};
            if (GetFileAttributesExW(world.c_str(), GetFileExInfoStandard, &fad)) {
              FILETIME nowFt;
              GetSystemTimeAsFileTime(&nowFt);
              if (isSaveFresh(ticksOf(nowFt), ticksOf(fad.ftLastWriteTime), kSaveFreshWindowSec)) {
                fresh = true;
                break;
              }
            }
            if (waitedMs >= kSaveConfirmTimeoutMs) break;
            Sleep(kSaveConfirmPollMs);
            waitedMs += kSaveConfirmPollMs;
          }
          if (fresh)
            line("World saved.");
          else
            line("No fresh save seen within 2 minutes - archiving the world file as it is.");
        }
      }
    }
    if (abort_.load()) {
      finish(false, "aborted", "");
      running_.store(false);
      return;
    }

    setPhase("zip");
    const BackupOutcome r =
        runBackup(db_, srv, cfg, [this](const std::string& t) { line(t); }, logSink_, &abort_);
    finish(r.ok, r.error, r.file);
  } catch (const std::exception& ex) {
    line(std::string("Error: ") + ex.what());
    log("normal", std::string("Map backup failed with an internal error: ") + ex.what());
    finish(false, ex.what(), "");
  } catch (...) {
    line("Error: unknown exception");
    finish(false, "unknown exception", "");
  }
  running_.store(false);
}

}  // namespace amucore
