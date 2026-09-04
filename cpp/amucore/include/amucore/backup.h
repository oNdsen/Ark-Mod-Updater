#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "amucore/db.h"
#include "amucore/supervisor.h"

// Automated map backups (AMU 2.3). One backup of one server =
//   [warn the players over RCON: the server's backup plan, warnplan.h format]
//   -> "saveworld" over RCON and wait for a fresh <map>.ark (running server)
//   -> zip the SavedArks tree into <backup dir>\<name>_<map>_<stamp>.zip
//      (worldbackup.h include rules: the live world, profiles, tribes)
//   -> rotation: keep the newest N archives of this server/map
//   -> stamp settings.last_backup.
// The Backuper runs that on ONE worker thread; the host polls status() like it
// does for the Orchestrator. runBackup() is the synchronous archive step alone
// - the orchestrator calls it in place of the old raw .ark copy ("backup
// before every update"), after the server has been stopped, so it needs no
// RCON. The interval logic (schedule.h intervalDue) lives in the host's
// scheduler tick.

namespace amucore {

struct BackupStatus {
  bool running = false;
  std::string phase;       // idle|countdown|save|zip|done|error
  int countdownSecs = -1;  // >= 0 while the player warning is ticking
  int64_t serverId = -1;   // the server of the running (or last) backup
  std::string lastFile;    // archive of the last successful backup (full path)
  std::string lastError;   // "" after a successful backup
  std::vector<std::string> lines;  // rolling console (newest LAST, capped)
};

struct BackupOutcome {
  bool ok = false;
  std::string file;   // full path of the archive
  std::string error;  // why not, when !ok
  int files = 0;
  uint64_t bytesIn = 0;
  uint64_t bytesOut = 0;
  int deleted = 0;  // rotated-out archives
};

// Where the archives of `srv` go: cfg.backupDir when set, else
// <install>\ShooterGame\Saved\Backups.
std::string backupFolder(const Server& srv, const Settings& cfg);

// SYNCHRONOUS archive step: zip + rotate + stamp. Progress goes to `line`
// (may be empty) and to `log` (the shared log sink; may be empty). The world
// must already be in the state the caller wants archived (fresh save, or a
// stopped server).
BackupOutcome runBackup(Db& db, const Server& srv, const Settings& cfg,
                        const std::function<void(const std::string&)>& line,
                        const Supervisor::LogSink& log, const std::atomic<bool>* abort = nullptr);

// "<map>.ark" of this install - the file whose last-write time tells whether
// a save is fresh. ASA keeps it in SavedArks\<map>\, ASE directly in SavedArks.
std::string worldFilePath(const std::string& installRoot, const std::string& map);

class Backuper {
 public:
  using LogSink = Supervisor::LogSink;

  Backuper(Db& db, Supervisor& sup, LogSink log = nullptr);
  ~Backuper();

  Backuper(const Backuper&) = delete;
  Backuper& operator=(const Backuper&) = delete;

  // Start a backup of one server on the worker thread. With `warn`, the
  // server's backup message plan runs first (only when the server is running
  // and RCON is enabled). Returns false while a backup is in progress.
  bool start(int64_t serverId, bool warn);

  // "Skip countdown": the current wait ends now; the remaining texts still go
  // out back to back before the save.
  void skipCountdown();

  bool running() const { return running_.load(); }

  // Abort a running backup (its partial archive is removed) and join the
  // worker. The destructor does the same; the host calls it first thing on
  // close so the supervisor shutdown never overlaps a backup.
  void shutdown();

  // Mutex-guarded snapshot for the UI poll.
  BackupStatus status();

 private:
  void run(int64_t serverId, bool warn);  // worker entry
  void line(const std::string& text);
  void setPhase(const char* phase);
  bool waitSecs(int secs, int extra);  // false when skipped/aborted

  Db& db_;
  Supervisor& sup_;
  LogSink logSink_;

  std::mutex mu_;
  BackupStatus st_;
  std::atomic<bool> running_{false};
  std::atomic<bool> skip_{false};
  std::atomic<bool> abort_{false};  // destructor only
  std::thread worker_;
};

}  // namespace amucore
