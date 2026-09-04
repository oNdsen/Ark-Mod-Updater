#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "amucore/db.h"
#include "amucore/supervisor.h"

// The mod-update orchestrator - the C++ port of _DownloadAndInstallMods +
// _Go4Update from amu.au3. One run does:
//   collect unique mod ids (ActiveMods of the target server/s)
//   -> write a steamcmd runscript (login anonymous | DPAPI-decrypted creds)
//   -> run steamcmd.exe +runscript, stream-parse stdout line by line
//      ("Success. Downloaded item <id> (<bytes>)" queues the mod for install)
//   -> per server: decide per mod if an install is needed (dir size vs cached
//      usize, timeupdated vs the ACF), then ONCE per server: countdown
//      broadcasts msg1/msg2/msg3 ({minutes} substituted) -> graceful stop via
//      Supervisor::stopForUpdate -> optional .ark backup -> per mod: unpack
//      (.z via zunpack, 3 retries) / copy files into a staging folder and swap
//      it over the old install only when EVERY file made it, write the .mod
//      descriptor (mod_writer) -> update the mods table (ONLY after a fully
//      successful install) -> restart via Supervisor::startAfterUpdate.
// The run executes on ONE worker thread owned by this class; the UI polls
// status() (same pattern as the Supervisor). Steamcmd output goes to the log
// sink as type "steamcmd" (error lines escalated to "normal"), exactly like
// the AutoIt _write_log usage.

namespace amucore {

struct OrchestratorConfig {
  // <exeDir>\lib - steamcmd at libDir\steamcmd\steamcmd.exe, its workshop
  // content at libDir\steamcmd\steamapps\workshop\content\346110\<modid>.
  std::string libDir;
};

// Live per-mod status, mirroring the AutoIt _ModStatus texts ("Downloading...",
// "Downloaded", "Installing...", "Updated", "Installed", "Up to date",
// "Removed from Workshop", "Not downloaded", "Error: <reason>"). A mod whose
// files could not be unpacked/copied gets "Error: install failed" - the UI paints
// every "Error*" status red - and its mods row is deliberately NOT stamped, so
// the next run retries instead of reporting the broken install as up to date.
struct ModProgress {
  std::string modId;
  std::string status;
};

struct UpdateStatus {
  bool running = false;
  std::string phase;       // idle|collect|download|install|countdown|done|error
  int countdownSecs = -1;  // >= 0 while the restart countdown is ticking
  std::vector<ModProgress> mods;
  std::vector<std::string> lines;  // rolling progress console (newest LAST, capped)
};

// --- pure helpers (unit-tested without steamcmd/filesystem) ------------------

// The steamcmd runscript content. `loginLine` is the full "login ..." line
// ("login anonymous" or "login user pass guard"); mod ids get one
// "workshop_download_item 346110 <id> validate" line each; ends with "quit".
// Lines are CRLF-terminated like the AutoIt FileWriteLine output.
// (Stdout parsing is NOT duplicated here - the run uses the existing
// amucore::SteamCmdParser, which already mirrors _ParseSteamCMDLine.)
std::string buildSteamcmdScript(const std::string& loginLine,
                                const std::vector<std::string>& modIds);

// The needs-install decision for an already-installed mod (port of the
// _Go4Update size/timestamp checks): true when the on-disk dir size differs
// from the cached usize OR the cached timeupdated differs from the ACF value,
// AND (dir size != freshly computed unpacked size OR timestamps differ).
bool needsReinstall(int64_t installedDirSize, int64_t cachedUsize,
                    const std::string& cachedTimeUpdated, const std::string& acfTimeUpdated,
                    int64_t unpackedSourceSize);

// SteamCMD watchdog: how long the child may go WITHOUT writing a single byte to
// stdout before AMU terminates it. steamcmd stays completely silent while one
// workshop item downloads, so the deadline measures time since the LAST output
// (not total runtime) and is deliberately generous - a multi-GB mod on a slow
// line is legitimately slow. Half an hour of complete silence, on the other
// hand, means the child is wedged (typically blocked on a console prompt that
// @NoPromptForPassword does not cover), which would otherwise hang the update -
// and, through the joining destructor, the whole app - forever.
constexpr uint64_t kSteamCmdIdleTimeoutMs = 30ull * 60 * 1000;

// How long to wait for a well-behaved steamcmd to exit once its stdout is at
// EOF, before it is terminated (it may still hold the credential-bearing
// runscript open, which has to be scrubbed).
constexpr uint32_t kSteamCmdExitWaitMs = 30000;

// PURE: the watchdog decision for the steamcmd stdout pump. Both values are
// GetTickCount64() milliseconds; a `now` that is not after `lastOutput` (clock
// oddity) never trips the deadline instead of underflowing.
bool steamcmdTimedOut(uint64_t lastOutputTickMs, uint64_t nowTickMs,
                      uint64_t idleTimeoutMs = kSteamCmdIdleTimeoutMs);

// PURE: the human-readable cause behind a failed .z file install. `zerr` is the
// last unpackZ error code; the retry loop only writes when unpackZ succeeded, so
// zerr == 0 means the decompression was fine and the WRITE failed (which the
// first port reported as the nonsensical "error 0").
std::string unpackFailureReason(int zerr);

// --- the orchestrator --------------------------------------------------------

class Orchestrator {
 public:
  // Uses the SAME LogSink signature as the Supervisor: (serverId, type, msg).
  using LogSink = Supervisor::LogSink;

  Orchestrator(Db& db, Supervisor& sup, OrchestratorConfig cfg, LogSink log = nullptr);
  ~Orchestrator();

  Orchestrator(const Orchestrator&) = delete;
  Orchestrator& operator=(const Orchestrator&) = delete;

  // Start an update run on the worker thread. serverId -1 = all servers;
  // onlyModIds "" = all active mods (of the target), else a comma-separated
  // id list (the UI sends "the missing ones" or "the selected one"). force:
  // reinstall mods the manifest reports as up to date (the UI's "Reinstall").
  // It only overrides that one decision - the RCON player warning still runs,
  // unlike the per-server `force` column inherited from the AutoIt app.
  // Returns false when a run is already in progress. Non-blocking.
  bool start(int64_t serverId, const std::string& onlyModIds = "", bool force = false);

  // User pressed "Skip Countdown": the current msg1/msg2 wait finishes
  // immediately (msg3 + stop still run, matching the AutoIt cancel button).
  // Consumed by ONE wait - a second countdown still runs in full.
  void skipCountdown();

  // True while the worker is running (cheap atomic read).
  bool running() const { return running_.load(); }

  // Mutex-guarded snapshot for the UI poll.
  UpdateStatus status();

 private:
  void run(int64_t serverId, std::string onlyModIds, bool force);  // worker entry
  // false when this server hit an install error (the run ends in phase "error").
  bool updateServer(const Server& srv,
                    const std::vector<std::pair<std::string, int64_t>>& downloaded,
                    const std::vector<std::string>& onlyModIds, bool force);
  void line(const std::string& text);                       // progress console
  void modStatus(const std::string& modId, const std::string& status);
  void setPhase(const char* phase);

  Db& db_;
  Supervisor& sup_;
  OrchestratorConfig cfg_;
  LogSink logSink_;

  std::mutex mu_;
  UpdateStatus st_;
  std::atomic<bool> running_{false};
  std::atomic<bool> skip_{false};
  // Hard cancel for the WHOLE run, set by the destructor only. Honoured by every
  // wait/poll the run does (countdowns, the steamcmd stdout pump - whose child is
  // terminated - and the per-server/per-mod loops) so ~Orchestrator's join cannot
  // block for minutes. Deliberately separate from skip_, which stays the
  // user-facing "Skip Countdown" and is consumed by a single wait.
  std::atomic<bool> abort_{false};
  std::thread worker_;
};

}  // namespace amucore
