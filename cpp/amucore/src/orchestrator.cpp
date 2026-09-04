#include "amucore/orchestrator.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <unordered_map>
#include <utility>

#include "amucore/backup.h"
#include "amucore/credstore.h"
#include "amucore/mod_writer.h"
#include "amucore/rcon.h"
#include "amucore/serverconfig.h"
#include "amucore/warnplan.h"
#include "amucore/steamcmd_parser.h"
#include "amucore/updatecheck.h"
#include "amucore/workshop.h"
#include "amucore/zunpack.h"

// Port of _DownloadAndInstallMods + _Go4Update + __ModDecomp from amu.au3.
// Documented deviations from the AutoIt original:
//   - the server is restarted via Supervisor::startAfterUpdate ONLY when it was
//     running before the update (AutoIt always ran the start script).
//   - the once-a-day cache-clear date is persisted in a marker file next to
//     steamcmd (the Db API cannot UPDATE the "-1" settings row msg1 yet).
//   - a mod is unpacked into a staging folder and swapped in only when every
//     file made it; a failed install keeps the previous one AND the stale mods
//     row, so the next run retries instead of reporting "up to date".
//   - the steamcmd child gets stdin at EOF plus an idle watchdog, and is
//     terminated when it goes silent or when the app is shutting down: no step
//     of a run may block ~Orchestrator's join indefinitely.

namespace amucore {

namespace fs = std::filesystem;

namespace {

// UTF-8 -> UTF-16 for Win32 / std::filesystem (same pattern as supervisor.cpp).
std::wstring toWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

std::string fromWide(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                              nullptr, nullptr);
  if (n <= 0) return std::string();
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                      nullptr);
  return s;
}

fs::path widePath(const std::string& utf8) { return fs::path(toWide(utf8)); }

std::string toLowerAscii(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

bool endsWithNoCase(const std::string& s, const std::string& suffix) {
  if (s.size() < suffix.size()) return false;
  return toLowerAscii(s.substr(s.size() - suffix.size())) == toLowerAscii(suffix);
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
  if (from.empty()) return s;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
}

std::string trimWs(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return std::string();
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

// Local date "YYYY/MM/DD" - the daily cache-clear stamp (task spec; AutoIt _NowDate).
std::string nowDateString() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  char buf[16];
  _snprintf_s(buf, _TRUNCATE, "%04u/%02u/%02u", st.wYear, st.wMonth, st.wDay);
  return std::string(buf);
}

std::vector<uint8_t> readFileBytes(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return {};
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

bool writeFileBytes(const fs::path& p, const std::vector<uint8_t>& bytes) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  if (!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size()));
  return f.good();
}

std::string readTextFile(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return std::string();
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool writeTextFile(const fs::path& p, const std::string& text) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  f.write(text.data(), static_cast<std::streamsize>(text.size()));
  return f.good();
}

// Recursive directory size in bytes (DirGetSize equivalent). 0 when absent.
int64_t dirSizeRecursive(const std::string& dirUtf8) {
  std::error_code ec;
  fs::path root = widePath(dirUtf8);
  if (!fs::exists(root, ec)) return 0;
  int64_t total = 0;
  fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    std::error_code fec;
    if (!it->is_regular_file(fec) || fec) continue;
    const auto sz = it->file_size(fec);
    if (!fec) total += static_cast<int64_t>(sz);
  }
  return total;
}

// Port of _GetUnpackedModSize (amu.au3 line 869): estimate the INSTALLED size of a
// downloaded mod without unpacking it. Prefers the WindowsNoEditor subdir; sums the
// plain sizes of non-.z files and the unpacked-size header field of .z files;
// *.uncompressed_size sidecars are excluded (the AutoIt read them instead of the .z
// headers - same value, the header needs no sidecar to be present).
int64_t unpackedModSize(const std::string& modSrcDirUtf8) {
  std::error_code ec;
  fs::path root = widePath(modSrcDirUtf8);
  if (fs::exists(root / L"WindowsNoEditor", ec)) root /= L"WindowsNoEditor";
  if (!fs::exists(root, ec)) return 0;
  int64_t total = 0;
  fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    std::error_code fec;
    if (!it->is_regular_file(fec) || fec) continue;
    const std::string name = fromWide(it->path().filename().wstring());
    if (endsWithNoCase(name, ".uncompressed_size")) continue;
    if (endsWithNoCase(name, ".z")) {
      std::ifstream f(it->path(), std::ios::binary);
      uint8_t hdr[32] = {};
      if (f.read(reinterpret_cast<char*>(hdr), sizeof(hdr))) {
        const int64_t u = zHeaderUnpackedSize(hdr, sizeof(hdr));
        if (u > 0) total += u;
      }
    } else {
      const auto sz = it->file_size(fec);
      if (!fec) total += static_cast<int64_t>(sz);
    }
  }
  return total;
}

// Callbacks so the file-scope install routine can reach the orchestrator's
// progress console and log sink without extra class members.
struct InstallHooks {
  std::function<void(const std::string&)> line;                    // progress console
  std::function<void(const char*, const std::string&)> log;       // (type, msg), serverId bound
};

// Outcome of one mod install. Ok is the ONLY result that may stamp the mods row
// as up to date (see updateServer) - anything else must keep the stale row so
// the next run retries.
enum class InstallResult { Ok, NoFiles, Failed };

// Suffix of the staging folder the files are unpacked into. It sits next to the
// live install (same parent -> same volume), so swapping it in is a rename.
constexpr const char* kStagingSuffix = ".amunew";

// Port of __ModDecomp (amu.au3 line 2445): install one downloaded mod from the
// steamcmd workshop content dir into <destRoot>\<modId> (+ <modId>.mod).
//
// DEVIATION from the AutoIt original (and from the first C++ port): the files go
// into a staging folder <destRoot>\<modId>.amunew and are swapped over the live
// install only when EVERY file made it. The original deleted the destination
// FIRST and merely logged per-file failures while still reporting success, so a
// failed unpack left a broken/empty mod folder that the caller then stamped as
// up to date - it was never repaired and the server was restarted onto it.
// The staging costs one mod's worth of extra disk space while unpacking; in
// exchange a failed install leaves the previous one completely untouched.
InstallResult installModFiles(const InstallHooks& h, const std::string& contentRootUtf8,
                              const std::string& destRootUtf8, const std::string& modId) {
  std::error_code ec;
  const std::string srcModUtf8 = contentRootUtf8 + "\\" + modId;
  fs::path srcDir = widePath(srcModUtf8);
  if (fs::exists(srcDir / L"WindowsNoEditor", ec)) srcDir /= L"WindowsNoEditor";

  const std::string destDirUtf8 = destRootUtf8 + "\\" + modId;
  const std::string destModUtf8 = destRootUtf8 + "\\" + modId + ".mod";
  const std::string stageDirUtf8 = destDirUtf8 + kStagingSuffix;
  const fs::path destDir = widePath(destDirUtf8);
  const fs::path destMod = widePath(destModUtf8);
  const fs::path stageDir = widePath(stageDirUtf8);

  // Collect all files recursively, excluding the *.uncompressed_size sidecars.
  // Done BEFORE anything is written: an incomplete download must not cost the
  // user the install he already has.
  std::vector<fs::path> rels;
  fs::recursive_directory_iterator it(srcDir, fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    std::error_code fec;
    if (!it->is_regular_file(fec) || fec) continue;
    const std::string name = fromWide(it->path().filename().wstring());
    if (endsWithNoCase(name, ".uncompressed_size")) continue;
    rels.push_back(it->path().lexically_relative(srcDir));
  }

  if (rels.empty()) {
    h.log("normal", "Mod " + modId + ": no unpackable files found in " +
                        fromWide(srcDir.wstring()) + " - skipped (incomplete download?).");
    h.line("Mod " + modId + ": no files to unpack - skipped.");
    return InstallResult::NoFiles;
  }

  // Fresh staging folder (a leftover from an interrupted run is discarded).
  fs::remove_all(stageDir, ec);
  ec.clear();
  fs::create_directories(stageDir, ec);
  if (ec) {
    h.log("normal", "Mod " + modId + ": could not create the staging folder " + stageDirUtf8 +
                        " (" + ec.message() + ") - install aborted.");
    h.line("Mod " + modId + ": install failed (staging folder) - keeping the previous install.");
    return InstallResult::Failed;
  }
  auto dropStaging = [&stageDir] {
    std::error_code rec;
    fs::remove_all(stageDir, rec);
  };

  for (const fs::path& rel : rels) {
    const fs::path src = srcDir / rel;
    const std::string relName = fromWide(rel.wstring());
    std::string failure;  // non-empty -> this file could not be installed
    if (endsWithNoCase(relName, ".z")) {
      // Unpack, stripping the .z extension from the destination name; up to 3
      // attempts like the AutoIt retry loop.
      std::wstring relW = rel.wstring();
      relW.resize(relW.size() - 2);  // drop ".z"
      const fs::path dest = stageDir / relW;
      std::error_code dec;
      fs::create_directories(dest.parent_path(), dec);
      int zerr = 0;
      bool ok = false;
      for (int attempt = 1; attempt <= 3 && !ok; ++attempt) {
        const auto blob = readFileBytes(src);
        const auto out = unpackZ(blob, &zerr);
        if (zerr == 0) ok = writeFileBytes(dest, out);
      }
      if (!ok)
        failure = "failed to unpack " + relName + " after 3 attempts (" +
                  unpackFailureReason(zerr) + ")";
    } else {
      const fs::path dest = stageDir / rel;
      std::error_code dec;
      fs::create_directories(dest.parent_path(), dec);
      fs::copy_file(src, dest, fs::copy_options::overwrite_existing, dec);
      if (dec) failure = "failed to copy " + relName + " (" + dec.message() + ")";
    }
    if (!failure.empty()) {
      // Fail fast: the staging folder is thrown away anyway, and the usual cause
      // (full disk, unreadable download) would fail every remaining file too.
      h.log("normal", "Mod " + modId + ": " + failure +
                          " - install aborted, the previous install was left in place.");
      h.line("Mod " + modId + ": " + failure + " - keeping the previous install.");
      dropStaging();
      return InstallResult::Failed;
    }
  }

  // Everything unpacked. Build the .mod descriptor from modmeta.info + mod.info
  // in the mod ROOT (NOT the WindowsNoEditor subdir - mirrors the AutoIt paths),
  // then swap the staging folder in.
  const auto modmeta = readFileBytes(widePath(srcModUtf8 + "\\modmeta.info"));
  const auto modinfo = readFileBytes(widePath(srcModUtf8 + "\\mod.info"));
  const uint64_t idNum = std::strtoull(modId.c_str(), nullptr, 10);
  const std::vector<uint8_t> modFileBytes =
      buildModFile(idNum, parseModInfo(modinfo), modmeta);

  if (fs::exists(destDir, ec)) {
    std::error_code dre;
    fs::remove_all(destDir, dre);
    std::error_code eec;
    if (dre || fs::exists(destDir, eec)) {
      // The old folder is only PARTIALLY gone here, so the install may well be
      // broken - reporting failure keeps the mods row stale and the next run
      // (or a Force Update) retries it.
      h.log("normal", "Mod " + modId + ": could not delete the old mod folder " + destDirUtf8 +
                          " (" + dre.message() + ") - install aborted, the next run retries it.");
      h.line("Mod " + modId + ": install failed (could not replace the mod folder).");
      dropStaging();
      return InstallResult::Failed;
    }
    h.log("debug", "Mod Folder Deleted: " + destDirUtf8);
  }
  std::error_code ren;
  fs::rename(stageDir, destDir, ren);
  if (ren) {
    h.log("normal", "Mod " + modId + ": could not move the unpacked files to " + destDirUtf8 +
                        " (" + ren.message() + ") - install aborted.");
    h.line("Mod " + modId + ": install failed (could not replace the mod folder).");
    dropStaging();
    return InstallResult::Failed;
  }

  if (fs::exists(destMod, ec)) {
    fs::remove(destMod, ec);
    h.log("debug", "Mod File Deleted: " + destModUtf8);
  }
  if (!writeFileBytes(destMod, modFileBytes)) {
    // Without the descriptor ARK does not load the mod - that is a failed
    // install, not a cosmetic problem.
    h.log("normal", "Mod " + modId + ": could not write " + destModUtf8 +
                        " - ARK would not load the mod, install reported as failed.");
    h.line("Mod " + modId + ": install failed (.mod descriptor).");
    return InstallResult::Failed;
  }
  h.log("debug", "ModFile created: " + destModUtf8);
  h.log("debug", "ModFolder created and Files unpacked: " + destDirUtf8);
  return InstallResult::Ok;
}

std::string gusIniPath(const std::string& serverPath) {
  return serverPath + "\\ShooterGame\\Saved\\Config\\WindowsServer\\GameUserSettings.ini";
}

}  // namespace

// --- pure helpers ------------------------------------------------------------

std::string buildSteamcmdScript(const std::string& loginLine,
                                const std::vector<std::string>& modIds) {
  std::string s;
  s += "@ShutdownOnFailedCommand 1\r\n";
  s += "@NoPromptForPassword 1\r\n";
  s += loginLine + "\r\n";
  for (const std::string& id : modIds) s += "workshop_download_item 346110 " + id + " validate\r\n";
  s += "quit\r\n";
  return s;
}

bool needsReinstall(int64_t installedDirSize, int64_t cachedUsize,
                    const std::string& cachedTimeUpdated, const std::string& acfTimeUpdated,
                    int64_t unpackedSourceSize) {
  // Mirrors the two-stage check in _Go4Update (amu.au3 2029 + 2040): a cheap
  // cached-size/timestamp gate first, then the freshly computed unpacked size.
  const bool timestampsDiffer = cachedTimeUpdated != acfTimeUpdated;
  if (installedDirSize == cachedUsize && !timestampsDiffer) return false;
  return installedDirSize != unpackedSourceSize || timestampsDiffer;
}

bool steamcmdTimedOut(uint64_t lastOutputTickMs, uint64_t nowTickMs, uint64_t idleTimeoutMs) {
  if (nowTickMs <= lastOutputTickMs) return false;  // no underflow on a clock oddity
  return (nowTickMs - lastOutputTickMs) >= idleTimeoutMs;
}

std::string unpackFailureReason(int zerr) {
  if (zerr != 0) return "unpack error " + std::to_string(zerr);
  return "could not write the unpacked file";
}

// --- the orchestrator ---------------------------------------------------------

Orchestrator::Orchestrator(Db& db, Supervisor& sup, OrchestratorConfig cfg, LogSink log)
    : db_(db), sup_(sup), cfg_(std::move(cfg)), logSink_(std::move(log)) {
  st_.phase = "idle";
}

Orchestrator::~Orchestrator() {
  // abort_ is the hard cancel every wait/poll of the run checks (countdowns, the
  // steamcmd stdout pump, the per-server/per-mod loops), so the join below cannot
  // hang for minutes. skip_ additionally shortens a countdown already in flight -
  // it is consumed by ONE wait, which is why it alone is not enough.
  abort_.store(true);
  skip_.store(true);
  if (worker_.joinable()) worker_.join();
}

bool Orchestrator::start(int64_t serverId, const std::string& onlyModIds, bool force) {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) return false;
  if (worker_.joinable()) worker_.join();  // reap the previous (finished) run
  {
    std::lock_guard<std::mutex> lk(mu_);
    st_ = UpdateStatus{};
    st_.running = true;
    st_.phase = "collect";
  }
  skip_.store(false);
  abort_.store(false);
  worker_ = std::thread(&Orchestrator::run, this, serverId, onlyModIds, force);
  return true;
}

void Orchestrator::skipCountdown() { skip_.store(true); }

UpdateStatus Orchestrator::status() {
  UpdateStatus out;
  {
    std::lock_guard<std::mutex> lk(mu_);
    out = st_;
  }
  out.running = running_.load();
  return out;
}

void Orchestrator::line(const std::string& text) {
  std::lock_guard<std::mutex> lk(mu_);
  st_.lines.push_back(text);
  while (st_.lines.size() > 300) st_.lines.erase(st_.lines.begin());
}

void Orchestrator::modStatus(const std::string& modId, const std::string& status) {
  std::lock_guard<std::mutex> lk(mu_);
  for (auto& m : st_.mods) {
    if (m.modId == modId) {
      m.status = status;
      return;
    }
  }
  st_.mods.push_back(ModProgress{modId, status});
}

void Orchestrator::setPhase(const char* phase) {
  std::lock_guard<std::mutex> lk(mu_);
  st_.phase = phase;
}

// Worker entry - the _DownloadAndInstallMods port.
void Orchestrator::run(int64_t serverId, std::string onlyModIds, bool force) {
  // Same comma-list grammar as ActiveMods= (trimmed, empties dropped).
  const std::vector<std::string> only = parseActiveMods(onlyModIds);
  auto log = [this](int64_t sid, const char* type, const std::string& msg) {
    if (logSink_) logSink_(sid, type, msg);
  };

  // The whole run body sits in a lambda so early returns still fall through to
  // the running_ reset below.
  auto body = [&] {
    setPhase("collect");

    // Friendly precondition FIRST: without SteamCMD there is nothing to run.
    // A missing steamcmd.exe is BOOTSTRAPPED from the GitHub release channel
    // (it is published as a flat asset next to the update files); steamcmd
    // then self-installs the rest of its files on its first run.
    {
      std::error_code ec;
      const std::string exe = cfg_.libDir + "\\steamcmd\\steamcmd.exe";
      if (!fs::exists(widePath(exe), ec)) {
        line("SteamCMD not found - downloading it from GitHub...");
        log(0, "normal", "SteamCMD not found - downloading it from GitHub.");
        fs::create_directories(widePath(cfg_.libDir + "\\steamcmd"), ec);
        const std::wstring url = std::wstring(kUpdateBasePath) + L"steamcmd.exe";
        bool ok = httpDownloadFile(kUpdateHost, url, exe);
        ec.clear();
        if (ok) ok = fs::exists(widePath(exe), ec) && fs::file_size(widePath(exe), ec) > 0;
        if (!ok) {
          std::error_code del;
          fs::remove(widePath(exe), del);  // discard a partial download
          const std::string msg =
              "Could not download steamcmd.exe - install the full AMU package "
              "or place SteamCMD at lib\\steamcmd\\. Update aborted.";
          line(msg);
          log(0, "normal", msg);
          setPhase("error");
          return;
        }
        line("SteamCMD downloaded - it will install itself on first run.");
        log(0, "debug", "SteamCMD bootstrapped from the GitHub release channel.");
      }
    }

    // Target server(s): -1 = all (the AutoIt 'all' mode).
    std::vector<Server> targets;
    for (const Server& s : db_.servers())
      if (serverId == -1 || s.id == serverId) targets.push_back(s);
    if (targets.empty()) {
      line("No matching server configured - nothing to update.");
      setPhase("done");
      return;
    }

    // Collect unique mod ids (order-preserving) from ActiveMods, or just the one.
    std::vector<std::string> modIds;
    auto addUnique = [&modIds](const std::string& id) {
      const std::string t = trimWs(id);
      if (t.empty()) return;
      for (const std::string& e : modIds)
        if (e == t) return;
      modIds.push_back(t);
    };
    if (!only.empty()) {
      for (const std::string& id : only) addUnique(id);
    } else {
      for (const Server& t : targets)
        for (const std::string& id : readActiveMods(gusIniPath(t.path))) addUnique(id);
    }
    if (modIds.empty()) {
      line("No active Mods found - nothing to update.");
      setPhase("done");
      return;
    }
    {
      std::string idList;
      for (size_t i = 0; i < modIds.size(); ++i) idList += (i ? ", " : "") + modIds[i];
      line("Mod IDs to check: " + idList);
    }

    // Login line: anonymous, or the stored SteamCMD account with the DPAPI
    // credentials decrypted (amu.au3 1536-1541; anonymous when the flag is set,
    // absent, or no user is stored).
    const Settings global = db_.settings(-1);
    std::string loginLine = "login anonymous";
    if (global.present && global.steamcmdAnonymous == 0 && !global.steamcmdUser.empty()) {
      loginLine = "login " + unprotect(global.steamcmdUser) + " " +
                  unprotect(global.steamcmdPass) + " " + unprotect(global.steamcmdGuard);
    }

    // Write the runscript to %TEMP% (it may hold credentials - scrubbed below).
    wchar_t tmpDir[MAX_PATH] = L"";
    wchar_t tmpFile[MAX_PATH] = L"";
    if (GetTempPathW(MAX_PATH, tmpDir) == 0 || GetTempFileNameW(tmpDir, L"amu", 0, tmpFile) == 0) {
      line("Error: could not create the SteamCMD script file.");
      log(0, "normal", "Error: could not create the SteamCMD script file.");
      setPhase("error");
      return;
    }
    const fs::path scriptPath(tmpFile);
    if (!writeTextFile(scriptPath, buildSteamcmdScript(loginLine, modIds))) {
      line("Error: could not create the SteamCMD script file.");
      log(0, "normal", "Error: could not create the SteamCMD script file.");
      setPhase("error");
      return;
    }
    // Overwrite the (possibly credential-bearing) script before deleting it.
    // Both steps can fail while steamcmd still holds the file open, so the result
    // is reported: with a non-anonymous login this file holds the SteamCMD user,
    // password and Steam Guard code in PLAINTEXT. The message never repeats the
    // content - only the path.
    const bool scriptHasCreds = (loginLine != "login anonymous");
    auto scrubAndDeleteScript = [&]() -> bool {
      std::string filler;
      for (int i = 0; i < 40; ++i) filler += std::string(64, '-') + "\r\n";
      const bool overwritten = writeTextFile(scriptPath, filler);
      std::error_code ec;
      fs::remove(scriptPath, ec);
      std::error_code eec;
      const bool gone = !fs::exists(scriptPath, eec);
      if (overwritten && gone) return true;
      const std::string path = fromWide(scriptPath.wstring());
      // Only a FAILED overwrite can leave credentials behind - once the file is
      // filled with dashes, a leftover temp file is harmless.
      const std::string what =
          !overwritten
              ? "Warning: could not overwrite the temporary SteamCMD script " + path +
                    (scriptHasCreds ? " - it may still hold your SteamCMD login on disk."
                                    : " (anonymous login - it holds no credentials).")
              : "Warning: could not delete the temporary SteamCMD script " + path +
                    " - its content was overwritten, so no credentials remain in it.";
      line(what);
      log(0, "normal", what);
      return false;
    };

    // Once-a-day Steam cache clear (amu.au3 1554-1568). The last-clear date lives
    // in settings(-1).msg1; the Db API cannot UPDATE that row yet, so a marker
    // file next to steamcmd persists the date instead (documented deviation).
    const std::string today = nowDateString();
    const fs::path markerPath = widePath(cfg_.libDir + "\\steamcmd\\amu_lastcacheclear.txt");
    const std::string lastDb = global.present ? global.msg1 : std::string();
    const std::string lastMarker = trimWs(readTextFile(markerPath));
    if (lastDb != today && lastMarker != today) {
      std::error_code ec;
      if (fs::exists(widePath(cfg_.libDir + "\\steamcmd\\steamapps\\workshop\\appworkshop_346110.acf"), ec)) {
        line("Clearing Steam Cache...");
        fs::remove_all(widePath(cfg_.libDir + "\\steamcmd\\steamapps\\workshop"), ec);
        line("Clearing Steam Cache done");
        log(0, "debug", "Cleared Steam Cache");
      }
      writeTextFile(markerPath, today);
    }

    // --- run steamcmd, stream-parse stdout ----------------------------------
    setPhase("download");
    line("Downloading Mods, please wait...");

    const std::string exeUtf8 = cfg_.libDir + "\\steamcmd\\steamcmd.exe";
    {
      std::error_code ec;
      if (!fs::exists(widePath(exeUtf8), ec)) {
        line("Error: steamcmd.exe not found at " + exeUtf8);
        log(0, "normal", "Error: steamcmd.exe not found at " + exeUtf8);
        scrubAndDeleteScript();
        setPhase("error");
        return;
      }
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE outRd = nullptr, outWr = nullptr, inRd = nullptr, inWr = nullptr;
    if (!CreatePipe(&outRd, &outWr, &sa, 0) || !CreatePipe(&inRd, &inWr, &sa, 0)) {
      if (outRd) CloseHandle(outRd);
      if (outWr) CloseHandle(outWr);
      line("Error: could not create pipes for SteamCMD.");
      scrubAndDeleteScript();
      setPhase("error");
      return;
    }
    SetHandleInformation(outRd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(inWr, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd = L"\"" + toWide(exeUtf8) + L"\" +runscript \"" + scriptPath.wstring() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = outWr;
    si.hStdError = outWr;
    si.hStdInput = inRd;
    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(outWr);
    CloseHandle(inRd);
    // Close the stdin WRITE end right away: a +runscript run never reads stdin,
    // and the child's stdin must be at EOF. Keeping this handle open (as the
    // first port did for the whole run) means any unexpected console read -
    // a Steam Guard question @NoPromptForPassword does not cover, a "press any
    // key" - blocks steamcmd forever, and with it the read loop below, the worker
    // thread and the joining destructor: AMU could then never exit.
    CloseHandle(inWr);
    if (!created) {
      CloseHandle(outRd);
      line("Error: could not start SteamCMD.");
      log(0, "normal", "Error: could not start SteamCMD (" + exeUtf8 + ").");
      scrubAndDeleteScript();
      setPhase("error");
      return;
    }

    // Mods already present in the mods table (download success INSERTs new ones).
    std::set<std::string> knownMods;
    for (const Mod& m : db_.mods()) knownMods.insert(std::to_string(m.modid));

    std::vector<std::pair<std::string, int64_t>> downloaded;  // (modId, bytes)
    SteamCmdParser parser;
    auto handleEvent = [&](const SteamCmdEvent& ev) {
      switch (ev.type) {
        case SteamCmdEvent::Type::Downloading:
          modStatus(ev.modId, "Downloading...");
          line("Downloading item " + ev.modId + " ...");
          break;
        case SteamCmdEvent::Type::DownloadSucceeded: {
          line("Success.");
          modStatus(ev.modId, "Downloaded");
          if (knownMods.find(ev.modId) == knownMods.end()) {
            Mod m;
            m.modid = std::strtoll(ev.modId.c_str(), nullptr, 10);
            m.size = ev.bytes;
            db_.upsertMod(m);
            knownMods.insert(ev.modId);
          }
          bool seen = false;
          for (const auto& d : downloaded)
            if (d.first == ev.modId) {
              seen = true;
              break;
            }
          if (!seen) downloaded.emplace_back(ev.modId, ev.bytes);
          break;
        }
        case SteamCmdEvent::Type::DownloadFailed:
          modStatus(ev.modId, ev.removed ? "Removed from Workshop" : "Error: " + ev.reason);
          line("Download failed for item " + ev.modId + ": " + ev.reason);
          break;
      }
    };

    // Stdout pump with a watchdog. ReadFile on an anonymous pipe has no timeout,
    // so the loop peeks first and only reads when bytes are pending; that keeps
    // three exit conditions available that a blocking read does not have:
    //   - the child is gone and the pipe is drained (a grandchild that inherited
    //     the write end - steamcmd spawns helpers - can no longer keep us stuck),
    //   - abort_ (AMU is shutting down),
    //   - kSteamCmdIdleTimeoutMs without a single byte of output (wedged child).
    // The last two kill the child; leaving it running would keep the credential
    // -bearing runscript locked and block the scrub below.
    constexpr DWORD kPumpPollMs = 100;
    std::string lastChunk;
    char buf[4096];
    DWORD n = 0;
    uint64_t lastOutputTick = GetTickCount64();
    bool stalled = false;
    bool cancelled = false;
    for (;;) {
      if (abort_.load()) {
        cancelled = true;
        break;
      }
      DWORD avail = 0;
      if (!PeekNamedPipe(outRd, nullptr, 0, nullptr, &avail, nullptr)) break;  // pipe broken
      if (avail == 0) {
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
          // Child exited: nothing can be written any more, so one more peek
          // tells us whether output written just before the exit is still queued.
          DWORD left = 0;
          if (!PeekNamedPipe(outRd, nullptr, 0, nullptr, &left, nullptr) || left == 0) break;
        } else {
          if (steamcmdTimedOut(lastOutputTick, GetTickCount64())) {
            stalled = true;
            break;
          }
          Sleep(kPumpPollMs);
          continue;
        }
      }
      if (!ReadFile(outRd, buf, sizeof(buf), &n, nullptr) || n == 0) break;
      lastOutputTick = GetTickCount64();
      const std::string chunk(buf, n);
      // Dedupe identical consecutive chunks; escalate error lines (amu.au3 1601-1615).
      if (chunk != lastChunk) {
        lastChunk = chunk;
        log(0, "steamcmd", "SteamCMD: " + chunk);
        const std::string lower = toLowerAscii(chunk);
        if (lower.find("error") != std::string::npos) {
          line(trimWs(chunk));
          log(0, "normal", "SteamCMD: " + chunk);
          if (lower.find("locking") != std::string::npos) {
            line("Locking Failed? Try to add your SteamCMD Login");
            log(0, "normal", "SteamCMD: Locking Failed? Try to add your SteamCMD Login");
          }
        }
      }
      for (const SteamCmdEvent& ev : parser.feed(chunk)) handleEvent(ev);
    }
    for (const SteamCmdEvent& ev : parser.flush()) handleEvent(ev);

    if (stalled) {
      const std::string msg =
          "SteamCMD produced no output for " +
          std::to_string(kSteamCmdIdleTimeoutMs / 60000) +
          " minutes - it looks stuck (waiting for a console input?). Terminating it.";
      line(msg);
      log(0, "normal", msg);
    }
    // Terminate the child when we gave up on it, and also when it outlived the
    // exit wait: an abandoned steamcmd keeps the runscript open, which makes the
    // credential scrub below fail silently.
    if (stalled || cancelled) TerminateProcess(pi.hProcess, 1);
    if (WaitForSingleObject(pi.hProcess, kSteamCmdExitWaitMs) != WAIT_OBJECT_0) {
      log(0, "normal", "SteamCMD did not exit after its output ended - terminating it.");
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, kSteamCmdExitWaitMs);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(outRd);

    Sleep(1000);  // AutoIt settle delay before touching the script file
    if (!scrubAndDeleteScript()) {
      // Whatever held the file open should be gone by now (the child is dead) -
      // one retry is enough, and the lambda logs if it fails again.
      Sleep(1000);
      scrubAndDeleteScript();
    }

    if (cancelled) {
      log(0, "normal", "Update run cancelled - AMU is shutting down.");
      setPhase("idle");
      return;
    }

    // --- per-server install --------------------------------------------------
    if (downloaded.empty()) {
      line("No Updates found.");
      setPhase(stalled ? "error" : "done");
      return;
    }
    setPhase("install");
    // A terminated steamcmd is a failed run even when the items it did finish
    // install cleanly - the user must see that not everything was downloaded.
    bool anyFailed = stalled;
    for (const Server& t : targets) {
      if (!updateServer(t, downloaded, only, force)) anyFailed = true;
      line("Finished Server ID " + std::to_string(t.id));
      if (abort_.load()) {
        log(0, "normal", "Update run cancelled - AMU is shutting down.");
        setPhase("idle");
        return;
      }
    }
    if (anyFailed) {
      line("Finished Updating Process - WITH ERRORS, see the log.");
      log(0, "normal", "Finished Update Process with errors - some mods were not installed.");
      setPhase("error");
      return;
    }
    line("Finished Updating Process.");
    log(0, "debug", "Finished Update Process.");
    setPhase("done");
  };

  // Nothing may escape the thread entry: an uncaught exception (bad_alloc, a
  // length_error from a corrupt archive, a filesystem error, std::stoll...) would
  // call std::terminate and take the whole app down mid-update - with the game
  // server already stopped. Report it, park the run in the "error" phase and let
  // the UI recover. The reporting itself must not throw out of the handler.
  auto reportCrash = [this, &log](const char* what) {
    try {
      line(std::string("Update aborted - internal error: ") + what);
      log(0, "normal", std::string("Update aborted - internal error: ") + what);
      setPhase("error");
    } catch (...) {
      // Out of memory while reporting out of memory - there is nothing left to do.
    }
  };
  try {
    body();
  } catch (const std::exception& ex) {
    reportCrash(ex.what());
  } catch (...) {
    reportCrash("unknown exception");
  }

  running_.store(false);
}

// The _Go4Update port: decide + countdown/stop + install + DB update + restart
// for one server. Returns false when at least one mod could not be installed (or
// the run threw) - the caller ends the run in the "error" phase.
bool Orchestrator::updateServer(const Server& srv,
                                const std::vector<std::pair<std::string, int64_t>>& downloaded,
                                const std::vector<std::string>& onlyModIds, bool force) {
  auto log = [this, &srv](const char* type, const std::string& msg) {
    if (logSink_) logSink_(srv.id, type, msg);
  };
  const std::string idStr = std::to_string(srv.id);
  const std::string gus = gusIniPath(srv.path);

  const bool rconEnabled =
      toLowerAscii(iniRead(gus, "ServerSettings", "RCONEnabled", "False")) == "true";
  std::string rconIp = iniRead(gus, "SessionSettings", "MultiHome", "0");
  const std::string rconPortStr = iniRead(gus, "ServerSettings", "RCONPort", "");
  const std::string rconPass = iniRead(gus, "ServerSettings", "ServerAdminPassword", "");
  const uint16_t rconPort =
      static_cast<uint16_t>(std::strtoul(rconPortStr.c_str(), nullptr, 10));
  if (rconEnabled && (rconIp == "0" || rconIp.empty())) {
    rconIp = "127.0.0.1";
    line("No IP for Server " + idStr + " found. Using " + rconIp);
    line("Please set an internal IP in your GameUserSettings.ini via: [SessionSettings] > MultiHome");
    log("debug", "No IP for Server " + idStr + " found (var MultiHome). Using " + rconIp);
  }

  if (srv.force == 1) {
    log("debug", "Force Update Option for Server " + idStr + " found.");
    line("Force Update Option Set!");
  }
  // Run-level force (the UI's "Reinstall") only overrides the needs-install
  // decision below; the per-server column additionally skips the RCON warning.
  const bool forceInstall = srv.force == 1 || force;
  if (force) line("Reinstall requested - up-to-date mods are reinstalled as well.");

  // Candidates: downloaded mods that are in this server's ActiveMods (the force
  // flag skips the needs-install decision below, not this membership filter).
  const std::vector<std::string> activeMods = readActiveMods(gus);
  std::vector<std::pair<std::string, int64_t>> candidates;
  for (const auto& d : downloaded) {
    if (!onlyModIds.empty()) {
      bool listed = false;
      for (const std::string& o : onlyModIds)
        if (o == d.first) { listed = true; break; }
      if (!listed) continue;
    }
    for (const std::string& a : activeMods)
      if (trimWs(a) == d.first) {
        candidates.push_back(d);
        break;
      }
  }
  if (candidates.empty()) {
    line("No Updates for Server " + idStr);
    return true;
  }

  std::unordered_map<std::string, Mod> modRows;
  for (const Mod& m : db_.mods()) modRows[std::to_string(m.modid)] = m;
  const std::string contentRoot = cfg_.libDir + "\\steamcmd\\steamapps\\workshop\\content\\346110";
  const std::string acfText =
      readTextFile(widePath(cfg_.libDir + "\\steamcmd\\steamapps\\workshop\\appworkshop_346110.acf"));
  const std::string destRoot = srv.path + "\\ShooterGame\\Content\\Mods";

  // Skip-aware wait that ticks countdownSecs once per second. The second is slept
  // in short slices so a Skip press - and the destructor's abort_ - take effect
  // right away instead of after a full tick. skip_ keeps its user-facing
  // semantics: it is CONSUMED by this wait (a later countdown runs in full),
  // while abort_ is sticky and ends every wait of the run.
  // Returns false when the wait was cut short by Skip or by abort_. `extra`
  // is added to the published countdownSecs so the UI shows the time until
  // the SHUTDOWN, not just until the next warning.
  auto waitSecs = [this](int secs, int extra) -> bool {
    constexpr int kSliceMs = 100;
    bool skipped = false;
    for (int remaining = secs; remaining > 0; --remaining) {
      if (abort_.load()) break;
      if (skip_.exchange(false)) { skipped = true; break; }
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
  };
  auto broadcast = [&](const std::string& msg, const char* which) {
    RconClient rc;
    std::string resp;
    const RconStatus rs = rc.runCommand(rconIp, rconPort, rconPass, "broadcast " + msg, resp);
    if (rs != RconStatus::Ok)
      log("normal", std::string("RCON broadcast (") + which + ") failed for Server " + idStr +
                        " - check RCONPort/ServerAdminPassword.");
  };

  bool shutdownDone = false;  // ONCE per server (the AutoIt $shutdown_procedure)
  bool wasRunning = false;
  bool ok = true;             // false once a mod could not be installed

  // The per-mod loop is exception-guarded so the restart tail below ALWAYS runs:
  // a throw in here (bad_alloc, a filesystem error, a corrupt archive) would
  // otherwise unwind past the restart and leave the game server stopped - the
  // update was the only reason it went down.
  try {
    for (const auto& [modId, bytes] : candidates) {
      if (abort_.load()) break;  // AMU is shutting down - take on no new mod
      const std::string srcModDir = contentRoot + "\\" + modId;
      std::error_code ec;
      if (!fs::exists(widePath(srcModDir), ec)) {
        // Guard at amu.au3 1986: downloaded content missing (removed/private mod).
        line("Mod " + modId + " was not downloaded (it may not exist or be private) - skipping.");
        log("normal", "Mod " + modId + " was not downloaded (may not exist/private) - skipped.");
        modStatus(modId, "Not downloaded");
        continue;
      }

      const AcfModInfo acf = parseAcfForMod(acfText, modId);
      // Normalize "" -> "0" so a missing ACF entry equals the 0 default the AutoIt
      // numeric comparison produced.
      const std::string acfTime = acf.timeUpdated.empty() ? "0" : acf.timeUpdated;

      const bool hasRow = modRows.find(modId) != modRows.end();
      const Mod cached = hasRow ? modRows[modId] : Mod{};
      const std::string installedDir = destRoot + "\\" + modId;
      const bool wasInstalled = fs::exists(widePath(installedDir), ec);

      bool doInstall = true;
      if (wasInstalled) {
        const int64_t oldSize = dirSizeRecursive(installedDir);
        const int64_t unpacked = unpackedModSize(srcModDir);
        doInstall = needsReinstall(oldSize, cached.usize, std::to_string(cached.timeupdated),
                                   acfTime, unpacked);
      }
      if (wasInstalled && !(doInstall || forceInstall)) {
        modStatus(modId, "Up to date");
        continue;
      }
      if (!(doInstall || forceInstall)) continue;

      // Workshop name/preview (network). On failure keep the cached values so a
      // Steam hiccup does not wipe the stored mod name (mirrors the neterror path).
      std::string modName = cached.name;
      std::string modPreview = cached.preview;
      {
        const WorkshopModInfo wi = getModInfo(std::strtoull(modId.c_str(), nullptr, 10));
        if (!wi.name.empty()) modName = wi.name;
        if (!wi.previewUrl.empty()) modPreview = wi.previewUrl;
      }
      if (modName.empty()) modName = "**Could not determine Mod Name**";

      // --- once per server: countdown broadcasts + graceful stop + backup -----
      if (!shutdownDone) {
        const uint32_t pid = detectPid(srv.path);
        if (pid != 0) {
          line("Found running ARK Server (ID: " + idStr + ") with PID " + std::to_string(pid));
          log("debug", "Found running ARK Server (ID: " + idStr + ") with PID " + std::to_string(pid));
        } else {
          line("No running Server Instance found (ID: " + idStr + "). Starting Update Process...");
          log("debug", "No running Server Instance found (ID: " + idStr + "). Starting Update Process...");
        }

        if (pid != 0) {
          // Pre-shutdown warnings: the per-server plan (warnplan.h). An older
          // database that never stored a plan gets the AutoIt msg1/msg2/msg3
          // columns as a plan, a fresh one the default plan. Steps run from
          // the largest minute mark down; a 0-minute step is sent right before
          // the stop. The per-server force flag skips all of it (AutoIt
          // semantics: force = no player warning).
          std::vector<WarnStep> plan = parseWarnPlan(srv.warnplan);
          if (plan.empty() && srv.warnplan.empty()) {
            plan = legacyWarnPlan(srv.restarttime, srv.msg1, srv.msg2, srv.msg3);
            if (plan.empty()) plan = defaultWarnPlan();
          }
          const std::vector<WarnStep> steps = countdownSteps(plan);
          if (srv.force == 0) {
            if (!rconEnabled) {
              line("RCON is disabled for server " + idStr + " - stopping without a player warning.");
            } else if (steps.empty()) {
              line("No shutdown warnings configured for server " + idStr + " - stopping right away.");
            } else {
              setPhase("countdown");
              // Skip: the remaining texts still go out (back to back, no waits)
              // so the players get the final notice - the AutoIt behaviour.
              bool skipped = false;
              for (size_t i = 0; i < steps.size() && !abort_.load(); ++i) {
                const std::string text = renderWarnText(steps[i].text, steps[i].minutes);
                line("RCON warning (" + std::to_string(steps[i].minutes) + " min): " + text);
                log("debug", "Sending RCON warning to " + rconIp + ": " + text);
                broadcast(text, ("warn" + std::to_string(steps[i].minutes)).c_str());
                if (skipped) continue;
                const int wait = secondsUntilNext(steps, i);
                const int after = (i + 1 < steps.size()) ? steps[i + 1].minutes * 60 : 0;
                if (wait > 0 && !waitSecs(wait, after)) skipped = true;
              }
              setPhase("install");
            }
          } else {
            line("Force Var found, skipping RCON Commands");
          }
          // A cancel during the countdown must not go on to stop the server:
          // stopForUpdate blocks for minutes (save confirm + graceful exit +
          // kill) and cannot be interrupted, and the destructor's join would
          // have to sit through all of it. Nothing has been touched yet here,
          // so leaving now is clean - the server keeps running.
          if (abort_.load()) break;
          if (rconEnabled && !steps.empty()) Sleep(500);  // let the last broadcast land
          // saveworld + fresh-save confirm + DoExit + terminate fallback. Only go
          // through the Supervisor when it actually sees the server up - calling
          // stopForUpdate on a tracked-but-stopped entry would leave its updating
          // flag set with no startAfterUpdate ever clearing it.
          const ServerStatus ss = sup_.status(srv.id);
          if (ss.running || ss.state == "starting") {
            line("Shutdown Server Instance " + idStr + " (PID: " + std::to_string(pid) + ")");
            log("debug", "Shutdown Server Instance " + idStr + " (PID: " + std::to_string(pid) + ")");
            const StopStatus st = sup_.stopForUpdate(srv.id, StopOptions{}, &wasRunning);
            // NoHandle means the stop could NOT end the process (typically AMU
            // unelevated vs an elevated server, or a kill that timed out). The
            // files must not be swapped underneath a live server - it has the
            // .pak files mapped, so the install would half-fail and the running
            // world would be left on a mix of old and new mods. Abort this
            // server. The restart tail at the end of this function is NOT
            // reached after `break` (shutdownDone stays false), so the
            // supervisor entry is re-armed right here: stopForUpdate left it
            // with `updating` set, which would otherwise keep crash detection,
            // the Start button and adoption dead until AMU is restarted.
            if (st == StopStatus::NoHandle) {
              const std::string msg =
                  "Server " + idStr + " could not be stopped (PID " + std::to_string(pid) +
                  " is still running) - skipping the mod swap so nothing is installed under a "
                  "live server. Stop it manually, or run AMU with the same rights as the server.";
              line(msg);
              log("normal", msg);
              sup_.startAfterUpdate(srv.id);  // the server still runs: this only re-arms the watcher
              ok = false;
              break;
            }
          } else {
            // Seen by pid but not known to the supervisor as running (an
            // external start inside one adoption sweep, or a stuck entry): the
            // same "never swap files under a live server" rule applies.
            const std::string msg =
                "Server " + idStr + " is running (PID " + std::to_string(pid) +
                ") but is not managed by the Supervisor, so it could not be stopped - "
                "skipping the mod swap. Stop it manually, or let AMU adopt it (a few seconds) "
                "and run the update again.";
            line(msg);
            log("normal", msg);
            ok = false;
            break;
          }
        }

        // Map backup before the install (runs whether or not the server was
        // running - AutoIt 2149). AMU 2.3: the same zip archive + rotation the
        // Automation tab's backups produce, in place of the raw .ark copy.
        if (srv.backup == 1) {
          setPhase("backup");
          const BackupOutcome b = runBackup(db_, srv, db_.settings(srv.id),
                                            [this](const std::string& t) { line(t); }, logSink_);
          if (!b.ok) line("Continuing with the update without a map backup.");
          setPhase("install");
        }
        shutdownDone = true;
      }

      // --- install this mod ----------------------------------------------------
      line("Updating ModID " + modId + "...");
      modStatus(modId, "Installing...");
      InstallHooks hooks;
      hooks.line = [this](const std::string& t) { line(t); };
      hooks.log = [&log](const char* type, const std::string& m) { log(type, m); };
      const InstallResult ir = installModFiles(hooks, contentRoot, destRoot, modId);
      if (ir == InstallResult::NoFiles) {
        modStatus(modId, "Not downloaded");
        continue;
      }
      if (ir == InstallResult::Failed) {
        // Do NOT touch the mods row: stamping the fresh timeupdated/usize here
        // would make needsReinstall report "up to date" forever and the broken
        // install would never be repaired (the bug this branch exists for).
        // installModFiles already logged the concrete cause.
        ok = false;
        modStatus(modId, "Error: install failed");
        log("normal", "Mod " + modId + " (" + modName + ") FAILED to install on Server " + idStr +
                          " - its cached state was left stale so the next run retries it.");
        continue;
      }
      log("normal", (wasInstalled ? std::string("Updated Mod ") : std::string("Installed Mod ")) +
                        modId + " (" + modName + ") on Server " + idStr);
      modStatus(modId, wasInstalled ? "Updated" : "Installed");

      // Update the mods row with a TARGETED write: `cached` is a snapshot from
      // the start of the run, and the Workshop backfill may have written
      // date/posted in the meantime - a full-row replace would revert them.
      Mod m = cached;
      m.modid = std::strtoll(modId.c_str(), nullptr, 10);
      m.size = bytes;
      m.usize = dirSizeRecursive(installedDir);
      m.name = modName;
      m.preview = modPreview;
      m.timeupdated = std::strtoll(acfTime.c_str(), nullptr, 10);
      db_.updateModInstall(m);
      modRows[modId] = m;
    }
  } catch (const std::exception& ex) {
    ok = false;
    line("Error while updating Server " + idStr + ": " + ex.what());
    log("normal", "Internal error while updating Server " + idStr + ": " + ex.what());
  } catch (...) {
    ok = false;
    line("Error while updating Server " + idStr + ": unknown exception");
    log("normal", "Internal error while updating Server " + idStr + ": unknown exception");
  }

  // Restart - DEVIATION from AutoIt (which always ran the start script): only
  // restart when the server was running before the update. shutdownDone (the
  // AutoIt $needupdate at decision time) marks that an install was attempted.
  // This runs after a failed/aborted install too: a server AMU stopped must not
  // stay down just because one mod could not be written.
  if (shutdownDone && wasRunning) {
    sup_.startAfterUpdate(srv.id);
    line("Started Server ID: " + idStr);
    log("debug", "Started Server ID: " + idStr);
  } else if (shutdownDone) {
    line("Server " + idStr + " was not running before the update - leaving it stopped");
    log("debug", "Server " + idStr + " was not running before the update - leaving it stopped");
  } else {
    line("No Updates for Server " + idStr);
  }
  return ok;
}

}  // namespace amucore
