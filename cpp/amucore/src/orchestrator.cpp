#include "amucore/orchestrator.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <unordered_map>
#include <utility>

#include "amucore/credstore.h"
#include "amucore/mod_writer.h"
#include "amucore/rcon.h"
#include "amucore/serverconfig.h"
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

// Local "DD.MM.YYYY_HH.MM.SS" for the .ark backup name (mirrors the AutoIt
// _DateTimeFormat(...,2) + (...,5) with / and : replaced by dots).
std::string backupStamp() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  char buf[32];
  _snprintf_s(buf, _TRUNCATE, "%02u.%02u.%04u_%02u.%02u.%02u", st.wDay, st.wMonth, st.wYear,
              st.wHour, st.wMinute, st.wSecond);
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

// Port of __ModDecomp (amu.au3 line 2445): install one downloaded mod from the
// steamcmd workshop content dir into <destRoot>\<modId> (+ <modId>.mod). Returns
// false when the source has no unpackable files (guard at amu.au3 2470-2477).
bool installModFiles(const InstallHooks& h, const std::string& contentRootUtf8,
                     const std::string& destRootUtf8, const std::string& modId) {
  std::error_code ec;
  const std::string srcModUtf8 = contentRootUtf8 + "\\" + modId;
  fs::path srcDir = widePath(srcModUtf8);
  if (fs::exists(srcDir / L"WindowsNoEditor", ec)) srcDir /= L"WindowsNoEditor";

  const std::string destDirUtf8 = destRootUtf8 + "\\" + modId;
  const std::string destModUtf8 = destRootUtf8 + "\\" + modId + ".mod";
  const fs::path destDir = widePath(destDirUtf8);
  const fs::path destMod = widePath(destModUtf8);

  if (fs::exists(destDir, ec)) {
    fs::remove_all(destDir, ec);
    h.log("debug", "Mod Folder Deleted: " + destDirUtf8);
  }
  if (fs::exists(destMod, ec)) {
    fs::remove(destMod, ec);
    h.log("debug", "Mod File Deleted: " + destModUtf8);
  }
  fs::create_directories(destDir, ec);

  // _CreateModFile FIRST (before the file loop), reading modmeta.info + mod.info
  // from the mod root (NOT the WindowsNoEditor subdir - mirrors the AutoIt paths).
  const auto modmeta = readFileBytes(widePath(srcModUtf8 + "\\modmeta.info"));
  const auto modinfo = readFileBytes(widePath(srcModUtf8 + "\\mod.info"));
  const uint64_t idNum = std::strtoull(modId.c_str(), nullptr, 10);
  writeFileBytes(destMod, buildModFile(idNum, parseModInfo(modinfo), modmeta));
  h.log("debug", "ModFile created: " + destModUtf8);

  // Collect all files recursively, excluding the *.uncompressed_size sidecars.
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
    return false;
  }

  for (const fs::path& rel : rels) {
    const fs::path src = srcDir / rel;
    const std::string relName = fromWide(rel.wstring());
    if (endsWithNoCase(relName, ".z")) {
      // Unpack, stripping the .z extension from the destination name; up to 3
      // attempts like the AutoIt retry loop, then continue with the next file.
      std::wstring relW = rel.wstring();
      relW.resize(relW.size() - 2);  // drop ".z"
      const fs::path dest = destDir / relW;
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
        h.log("normal", "Mod " + modId + ": failed to unpack " + relName +
                            " after 3 attempts (error " + std::to_string(zerr) + ").");
    } else {
      const fs::path dest = destDir / rel;
      std::error_code dec;
      fs::create_directories(dest.parent_path(), dec);
      fs::copy_file(src, dest, fs::copy_options::overwrite_existing, dec);
      if (dec)
        h.log("normal", "Mod " + modId + ": failed to copy " + relName + " (" +
                            dec.message() + ").");
    }
  }
  h.log("debug", "ModFolder created and Files unpacked: " + destDirUtf8);
  return true;
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

// --- the orchestrator ---------------------------------------------------------

Orchestrator::Orchestrator(Db& db, Supervisor& sup, OrchestratorConfig cfg, LogSink log)
    : db_(db), sup_(sup), cfg_(std::move(cfg)), logSink_(std::move(log)) {
  st_.phase = "idle";
}

Orchestrator::~Orchestrator() {
  skip_.store(true);  // shorten a countdown that is in flight
  if (worker_.joinable()) worker_.join();
}

bool Orchestrator::start(int64_t serverId, const std::string& onlyModId) {
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
  worker_ = std::thread(&Orchestrator::run, this, serverId, onlyModId);
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
void Orchestrator::run(int64_t serverId, std::string onlyModId) {
  auto log = [this](int64_t sid, const char* type, const std::string& msg) {
    if (logSink_) logSink_(sid, type, msg);
  };

  // The whole run body sits in a lambda so early returns still fall through to
  // the running_ reset below.
  [&] {
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
    if (!onlyModId.empty()) {
      addUnique(onlyModId);
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
    auto scrubAndDeleteScript = [&scriptPath] {
      std::string filler;
      for (int i = 0; i < 40; ++i) filler += std::string(64, '-') + "\r\n";
      writeTextFile(scriptPath, filler);
      std::error_code ec;
      fs::remove(scriptPath, ec);
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
    if (!created) {
      CloseHandle(outRd);
      CloseHandle(inWr);
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

    std::string lastChunk;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(outRd, buf, sizeof(buf), &n, nullptr) && n > 0) {
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

    WaitForSingleObject(pi.hProcess, 30000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(outRd);
    CloseHandle(inWr);

    Sleep(1000);  // AutoIt settle delay before touching the script file
    scrubAndDeleteScript();

    // --- per-server install --------------------------------------------------
    if (downloaded.empty()) {
      line("No Updates found.");
      setPhase("done");
      return;
    }
    setPhase("install");
    for (const Server& t : targets) {
      updateServer(t, downloaded, onlyModId);
      line("Finished Server ID " + std::to_string(t.id));
    }
    line("Finished Updating Process.");
    log(0, "debug", "Finished Update Process.");
    setPhase("done");
  }();

  running_.store(false);
}

// The _Go4Update port: decide + countdown/stop + install + DB update + restart
// for one server.
void Orchestrator::updateServer(const Server& srv,
                                const std::vector<std::pair<std::string, int64_t>>& downloaded,
                                const std::string& onlyModId) {
  auto log = [this, &srv](const char* type, const std::string& msg) {
    if (logSink_) logSink_(srv.id, type, msg);
  };
  const std::string idStr = std::to_string(srv.id);
  const std::string gus = gusIniPath(srv.path);
  const std::string minutes = std::to_string(srv.restarttime);
  const std::string msg1 = replaceAll(srv.msg1, "{minutes}", minutes);
  const std::string msg2 = replaceAll(srv.msg2, "{minutes}", minutes);
  const std::string msg3 = replaceAll(srv.msg3, "{minutes}", minutes);

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

  // Candidates: downloaded mods that are in this server's ActiveMods (the force
  // flag skips the needs-install decision below, not this membership filter).
  const std::vector<std::string> activeMods = readActiveMods(gus);
  std::vector<std::pair<std::string, int64_t>> candidates;
  for (const auto& d : downloaded) {
    if (!onlyModId.empty() && d.first != onlyModId) continue;
    for (const std::string& a : activeMods)
      if (trimWs(a) == d.first) {
        candidates.push_back(d);
        break;
      }
  }
  if (candidates.empty()) {
    line("No Updates for Server " + idStr);
    return;
  }

  std::unordered_map<std::string, Mod> modRows;
  for (const Mod& m : db_.mods()) modRows[std::to_string(m.modid)] = m;
  const std::string contentRoot = cfg_.libDir + "\\steamcmd\\steamapps\\workshop\\content\\346110";
  const std::string acfText =
      readTextFile(widePath(cfg_.libDir + "\\steamcmd\\steamapps\\workshop\\appworkshop_346110.acf"));
  const std::string destRoot = srv.path + "\\ShooterGame\\Content\\Mods";

  // Skip-aware wait that ticks countdownSecs once per second.
  auto waitSecs = [this](int secs) {
    for (int remaining = secs; remaining > 0; --remaining) {
      if (skip_.exchange(false)) break;
      {
        std::lock_guard<std::mutex> lk(mu_);
        st_.countdownSecs = remaining;
      }
      Sleep(1000);
    }
    std::lock_guard<std::mutex> lk(mu_);
    st_.countdownSecs = -1;
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

  for (const auto& [modId, bytes] : candidates) {
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
    if (wasInstalled && !(doInstall || srv.force == 1)) {
      modStatus(modId, "Up to date");
      continue;
    }
    if (!(doInstall || srv.force == 1)) continue;

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
        if (srv.force == 0) {
          if (rconEnabled) {
            line("Sending RCON MSG1: " + msg1);
            line("Time left for MSG2: " + std::to_string(srv.restarttime - 1) + " Minutes");
            log("debug", "Sending RCON MSG1 to " + rconIp + ": " + msg1);
            broadcast(msg1, "msg1");
            setPhase("countdown");
            waitSecs(srv.restarttime * 60 - 60);
            line("Sending RCON MSG2: " + msg2);
            log("debug", "Sending RCON MSG2 to " + rconIp + ": " + msg2);
            line("Time left for MSG3: 1 Minute");
            broadcast(msg2, "msg2");
            waitSecs(60);
            setPhase("install");
          }
        } else {
          line("Force Var found, skipping RCON Commands");
        }
        if (rconEnabled) {
          line("Sending RCON MSG3: " + msg3);
          log("debug", "Sending RCON MSG3 to " + rconIp + ": " + msg3);
          broadcast(msg3, "msg3");
          Sleep(500);
        }
        // saveworld + fresh-save confirm + DoExit + terminate fallback. Only go
        // through the Supervisor when it actually sees the server up - calling
        // stopForUpdate on a tracked-but-stopped entry would leave its updating
        // flag set with no startAfterUpdate ever clearing it.
        const ServerStatus ss = sup_.status(srv.id);
        if (ss.running || ss.state == "starting") {
          line("Shutdown Server Instance " + idStr + " (PID: " + std::to_string(pid) + ")");
          log("debug", "Shutdown Server Instance " + idStr + " (PID: " + std::to_string(pid) + ")");
          sup_.stopForUpdate(srv.id, StopOptions{}, &wasRunning);
        } else {
          log("normal", "Server " + idStr + " is running (PID " + std::to_string(pid) +
                            ") but not managed by the Supervisor - could not stop it for "
                            "the update.");
        }
      }

      // .ark backup (runs whether or not the server was running - AutoIt 2149).
      if (srv.backup == 1) {
        const std::string savedArks = srv.path + "\\ShooterGame\\Saved\\SavedArks\\";
        const fs::path arkFile = widePath(savedArks + srv.map + ".ark");
        if (fs::exists(arkFile, ec)) {
          const std::string bakName = srv.map + "_ModBak_" + backupStamp() + ".ark";
          fs::copy_file(arkFile, widePath(savedArks + bakName),
                        fs::copy_options::overwrite_existing, ec);
          line("Map Backup Successfull: " + bakName);
          log("debug", "Map Backup Successfull: " + bakName);
        } else {
          line("Error: Could not backup the Map. It seems that " + srv.map + " doesnt exists!");
          log("normal", "Error: Could not backup the Map. It seems that " + srv.map + " doesnt exists!");
        }
      }
      shutdownDone = true;
    }

    // --- install this mod ----------------------------------------------------
    line("Updating ModID " + modId + "...");
    modStatus(modId, "Installing...");
    InstallHooks hooks;
    hooks.line = [this](const std::string& t) { line(t); };
    hooks.log = [&log](const char* type, const std::string& m) { log(type, m); };
    if (!installModFiles(hooks, contentRoot, destRoot, modId)) {
      modStatus(modId, "Not downloaded");
      continue;
    }
    log("normal", (wasInstalled ? std::string("Updated Mod ") : std::string("Installed Mod ")) +
                      modId + " (" + modName + ") on Server " + idStr);
    modStatus(modId, wasInstalled ? "Updated" : "Installed");

    // Update the mods row (read-modify-write keeps olddate/date/manifest intact).
    Mod m = cached;
    m.modid = std::strtoll(modId.c_str(), nullptr, 10);
    m.size = bytes;
    m.usize = dirSizeRecursive(installedDir);
    m.name = modName;
    m.preview = modPreview;
    m.timeupdated = std::strtoll(acfTime.c_str(), nullptr, 10);
    db_.upsertMod(m);
    modRows[modId] = m;
  }

  // Restart - DEVIATION from AutoIt (which always ran the start script): only
  // restart when the server was running before the update. shutdownDone (the
  // AutoIt $needupdate at decision time) marks that an install was attempted.
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
}

}  // namespace amucore
