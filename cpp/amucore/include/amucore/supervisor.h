#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "amucore/launchspec.h"

// Process lifecycle manager for ARK dedicated servers (ASE + ASA). Replaces the
// AutoIt _RunScript start path and folds _Go4Update's stop sequence (saveworld ->
// fresh-save confirm -> kill) into a reusable core, ADDING RCON DoExit before the
// terminate fallback, plus crash detection with desired-state auto-restart.
//
// Architecture: the supervisor knows NOTHING about the DB or ini files. The host
// (main_sciter) supplies two providers - one builds the LaunchCommand for a
// server (reading db launch config + GameUserSettings.ini live, so an ini edit
// is picked up on the next start), one supplies the RCON endpoint for stops.
// The pure decision logic (decide/backoff/imageMatchesInstall/fileAge) is
// exposed free-standing for doctest coverage without any live process.

namespace amucore {

// --- desired-state model -----------------------------------------------------

enum class Desired { Stopped, Running };
enum class Actual { Stopped, Starting, Running, Stopping, Crashed, FailedToStart };

// What the watcher should do for one server on one tick.
enum class SupAction {
  None,
  Start,            // desired Running, actual Stopped (or Crashed w/ backoff done)
  Stop,             // desired Stopped, actual Running (alive)
  MarkStopped,      // process gone while desired Stopped -> normal stop, no restart
  ScheduleRestart,  // crash detected -> set Crashed + nextRestartAt (backoff)
  GiveUp,           // crash with failures >= maxRetries -> FailedToStart
  ResetFailures,    // healthy uptime reached -> failures = 0
};

struct RestartPolicy {
  int maxRetries = 5;          // consecutive failures before giving up
  int baseBackoffMs = 5000;    // 5s, doubling per failure
  int maxBackoffMs = 300000;   // capped at 5 min
  int healthyResetSec = 300;   // uptime that resets the failure counter
};

// PURE inputs for one server on one watcher tick.
struct DecideInput {
  Desired desired = Desired::Stopped;
  Actual actual = Actual::Stopped;
  bool processAlive = false;
  bool updating = false;     // orchestrator update in progress -> always None
  int failures = 0;
  int64_t uptimeSec = 0;     // seconds since start (valid when Running)
  bool backoffElapsed = false;  // when Crashed: nextRestartAt has passed
};

// PURE state-transition table (the heart of crash-vs-user-stop). Unit-tested
// exhaustively; the watcher merely executes what this returns.
SupAction decide(const DecideInput& in, const RestartPolicy& pol);

// PURE: min(baseBackoffMs * 2^failures, maxBackoffMs), failures clamped >= 0.
int backoffMs(int failures, const RestartPolicy& pol);

// PURE: does a process image path (e.g. "D:\ARK\srv1\ShooterGame\Binaries\
// Win64\ShooterGameServer.exe") belong to this install root's binaries dir?
// Case-insensitive, tolerates mixed / \ separators and a trailing separator.
// Compares the image's DIRECTORY (not filename) so ASE and ASA both match.
bool imageMatchesInstall(const std::wstring& imagePath, const std::string& installRoot);

// PURE: age in seconds between two FILETIME values expressed as 100ns ticks
// (positive when `nowTicks` is later). Correct across minute/hour/day rollovers
// - the bug class the AutoIt string-subtraction version had.
int64_t fileAgeSeconds(int64_t nowTicks, int64_t lastWriteTicks);

// PURE: fresh == 0 <= age <= windowSec (the AutoIt "<60s" contract).
bool isSaveFresh(int64_t nowTicks, int64_t lastWriteTicks, int windowSec = 60);

// --- stop sequence -----------------------------------------------------------

struct RconEndpoint {
  std::string host;      // MultiHome ip, or "127.0.0.1" when unset
  uint16_t port = 0;     // RCONPort
  std::string password;  // ServerAdminPassword
  bool enabled = false;  // RCONEnabled
};

struct StopOptions {
  bool sendSaveWorld = true;
  bool confirmFreshSave = true;
  bool sendDoExit = true;  // false -> straight to the terminate fallback
};

enum class StopStatus { CleanExit, TerminatedAfterTimeout, AlreadyStopped, NoHandle };

// Named, tunable timeouts (values mirror the AutoIt where one existed).
constexpr int kSaveConfirmPollMs = 1000;
constexpr int kSaveConfirmTimeoutMs = 120000;  // AutoIt 2-min hard timeout
constexpr int kSaveFreshWindowSec = 60;        // AutoIt "<60s" freshness window
constexpr int kGracefulExitMs = 60000;         // wait after DoExit
constexpr int kKillWaitMs = 5000;              // reap after TerminateProcess
constexpr int kStartupGraceMs = 5000;          // instant-exit detection window
constexpr int kWatcherPollMs = 3000;           // adoption sweep cadence

// --- detection ---------------------------------------------------------------

struct DetectedProc {
  uint32_t pid = 0;
  ArkGame game = ArkGame::ASE;  // from the exe name
  std::wstring imagePath;       // full exe path (diagnostics)
};

// One Toolhelp32 snapshot over all ShooterGameServer.exe / ArkAscendedServer.exe
// processes. `pid` is 0-free; entries whose image path could not be read are
// skipped. Free-standing so the host can probe without a Supervisor instance.
std::vector<DetectedProc> detectAllArkProcesses();

// The pid of the server running out of `installRoot`, or 0 (mirrors
// _CheckServerRunning's 0 == not-running contract).
uint32_t detectPid(const std::string& installRoot);

// --- live status snapshot ------------------------------------------------------

struct ServerStatus {
  int64_t serverId = 0;
  bool running = false;   // actual == Running
  uint32_t pid = 0;       // 0 when not running
  std::string state;      // "stopped"|"starting"|"running"|"stopping"|"crashed"|"failed"
  std::string desired;    // "running"|"stopped"
  int failures = 0;
  std::string detail;     // one-line human text for the UI status card
};

// --- the supervisor ------------------------------------------------------------

// Owns ONE watcher thread. Start/Stop requests flip desired-state and wake the
// watcher; blocking work (CreateProcessW, multi-minute stops) runs on detached
// worker threads so one server can never freeze detection for the others.
class Supervisor {
 public:
  // Builds the launch command for a server at (re)start time - reads db+ini live.
  using LaunchProvider = std::function<LaunchCommand(int64_t serverId)>;
  // Supplies the RCON endpoint for the graceful-stop sequence.
  using RconProvider = std::function<RconEndpoint(int64_t serverId)>;
  // Receives lifecycle events for the app log: (serverId, type, message) with
  // type "normal" or "debug" (matches the logs table convention).
  using LogSink = std::function<void(int64_t, const std::string&, const std::string&)>;

  Supervisor() = default;
  ~Supervisor();

  Supervisor(const Supervisor&) = delete;
  Supervisor& operator=(const Supervisor&) = delete;

  // Wire the providers, then spin up the watcher thread. Idempotent.
  void start(LaunchProvider launch, RconProvider rcon, LogSink log = nullptr);

  // Stop the watcher, close all owned process handles. Running game servers are
  // NOT touched (AMU quitting must never kill the game servers).
  void shutdown();

  // Register a server (from the DB). Safe to call again to update path/map;
  // autoRestart mirrors the launch-table toggle. Adopted processes (already
  // running out of installRoot) are picked up by the next watcher sweep.
  void track(int64_t serverId, const std::string& installRoot, const std::string& map,
             ArkGame game, bool autoRestart);
  void untrack(int64_t serverId);

  // UI actions - non-blocking (flip desired + wake the watcher).
  void requestStart(int64_t serverId);
  void requestStop(int64_t serverId);
  void requestRestart(int64_t serverId);  // stop (graceful) then start again

  // Orchestrator hooks: stopForUpdate suppresses the crash rule + auto-restart
  // while the mod files are being swapped; startAfterUpdate re-enables and
  // starts. wasRunning (out) lets the caller restore the prior state only.
  StopStatus stopForUpdate(int64_t serverId, const StopOptions& opt, bool* wasRunning);
  void startAfterUpdate(int64_t serverId);

  // Fast mutex-guarded snapshot reads for the UI poll.
  ServerStatus status(int64_t serverId);
  std::vector<ServerStatus> statusAll();

 private:
  struct Managed {
    int64_t serverId = 0;
    std::string installRoot;
    std::string map;
    ArkGame game = ArkGame::ASE;
    bool autoRestart = false;
    Desired desired = Desired::Stopped;
    Actual actual = Actual::Stopped;
    bool updating = false;
    bool busy = false;          // a worker is starting/stopping this server
    int failures = 0;
    uint32_t pid = 0;
    void* hProcess = nullptr;   // owned HANDLE (null for adopted-without-handle)
    int64_t startedAtTicks = 0;      // 100ns ticks (FILETIME epoch) at start
    int64_t nextRestartAtTicks = 0;  // when Crashed: earliest restart time
    uint32_t lastExitCode = 0;
    std::string detail;
  };

  void watcherLoop();
  void doStart(int64_t serverId);                          // worker
  void doStop(int64_t serverId, const StopOptions& opt);   // worker
  Managed* find(int64_t serverId);                          // callers hold mu_
  void log(int64_t serverId, const char* type, const std::string& msg);

  std::mutex mu_;
  std::vector<Managed> servers_;
  LaunchProvider launchProvider_;
  RconProvider rconProvider_;
  LogSink logSink_;
  RestartPolicy policy_;
  void* wakeEvent_ = nullptr;  // HANDLE
  void* stopEvent_ = nullptr;  // HANDLE
  std::thread watcher_;
  bool running_ = false;
};

}  // namespace amucore
