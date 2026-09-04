#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// SQLite data layer over the existing amu.db. Mirrors the AutoIt schema in
// amu.au3 (CREATE TABLE servers/settings/mods/logs) and the add-missing-column
// pattern of _CheckDBColumnAndCreate. All SQL uses prepared statements with
// bound parameters - never string concatenation.
struct sqlite3;  // opaque; avoids leaking <sqlite3.h> into consumers

#include "amucore/schedule.h"

namespace amucore {

// One row of "SELECT * FROM servers JOIN settings ON id = server_id" - the
// $aSQLArray the AutoIt GUI drives every tab from. Field order follows the
// $eID, $eNAME, $ePATH ... enum (amu.au3 line 228). serverId duplicates id
// (it is the settings.server_id join key, kept for fidelity with the enum).
struct Server {
  int64_t id = 0;             // $eID       servers.id (AUTOINCREMENT)
  std::string name;           // $eNAME     servers.name
  std::string path;           // $ePATH     servers.path (UNIQUE) - ARK install dir
  std::string startscript;    // $eSTARTSCRIPT servers.startscript
  std::string map;            // $eMAP      servers.map
  int64_t serverId = 0;       // $eSERVERID settings.server_id (== id)
  int debug = 0;              // $eDEBUG    settings.debug
  int backup = 0;             // $EBACKUP   settings.backup
  int force = 0;              // $EFORCE    settings.force
  int restarttime = 0;        // $eRESTARTTIME settings.restarttime
  std::string msg1;           // $eMSG1     settings.msg1
  std::string msg2;           // $eMSG2     settings.msg2
  std::string msg3;           // $eMSG3     settings.msg3
  std::string warnplan;       // settings.warnplan (AMU 2.2, warnplan.h format); "" = not stored
};

// One "-1" settings row is the app-global settings (no server). settings(server_id,...)
// with server_id == -1 (amu.au3 line 163). Same columns as the per-server settings.
struct Settings {
  int64_t serverId = -1;
  int debug = 0;
  int backup = 0;
  int force = 0;
  int restarttime = 0;
  std::string msg1;
  std::string msg2;
  std::string msg3;
  std::string warnplan;   // AMU 2.2 pre-shutdown warning plan (warnplan.h)
  int steamcmdAnonymous = 0;
  // The AutoIt stores DPAPI-encrypted STRINGS ("DPAPI:0x...") in these columns
  // (the INTEGER column type is only SQLite type *affinity* - text survives
  // as-is). Read as text; decrypt via credstore::unprotect where needed.
  std::string steamcmdUser;
  std::string steamcmdPass;
  std::string steamcmdGuard;
  bool present = false;   // false when no row exists for this server_id
};

// mods table row (amu.au3 line 181 / on-disk amu.db).
struct Mod {
  int64_t modid = 0;
  int64_t size = 0;
  std::string name;
  int64_t usize = 0;
  std::string preview;
  std::string olddate;
  std::string date;
  std::string posted;  // Workshop "Posted" (first published); column added 2.0
  int64_t manifest = 0;
  int64_t timeupdated = 0;
};

// logs table row (amu.au3 line 184).
struct LogEntry {
  int64_t logId = 0;
  int64_t serverId = 0;
  std::string type;
  std::string date;
  std::string entry;
};

// launch table row (C++ rewrite; replaces the AutoIt per-server startscript with
// AMU-managed launch config). 1:1 with servers.id. Everything the configurator
// also owns (ports/RCON/session/mods) stays in GameUserSettings.ini and is NOT
// stored here - only launch-orchestration state that has no ini home. Defaults
// match the DDL so an absent row and a default struct build the same command.
struct LaunchConfig {
  int64_t serverId = 0;
  std::string game = "ASE";   // "ASE" | "ASA" - selects exe + arg dialect
  int desiredState = 0;       // 0 = Stopped, 1 = Running (supervisor target)
  int autoRestart = 0;        // restart on unexpected exit
  int autoStart = 0;          // request a start for this server when AMU launches
  std::string clusterId;      // -clusterid=   ("" = standalone)
  std::string clusterDir;     // -ClusterDirOverride=
  int battleye = 1;           // 1 = on (default), 0 -> -NoBattlEye
  int crossplay = 0;          // ASA -> -ServerPlatform=ALL when 1
  int autoManaged = 0;        // ASE -> -automanagedmods
  std::string extraArgs;      // verbatim tail appended to argv
  std::string extraFlags;     // verbatim dash flags
  std::string perfFlags;      // e.g. "-USEALLAVAILABLECORES"
  int maxPlayers = 0;         // 0 = omit (defer to ini); >0 overrides
  std::string updatedAt;      // last-write "YYYY/MM/DD HH:MM:SS"
  bool present = false;       // false when no row exists (all-defaults applies)
};

// Thread safety: ONE Db is shared by the Sciter UI thread, the supervisor's
// watcher + worker threads and the orchestrator's worker thread (main_sciter.cpp
// hands the same object to both, and the log sink fires from the background
// threads). The SQLite amalgamation is built serialized, so a single sqlite3_*
// call is safe on its own - but our multi-call sequences are not: an interleaved
// INSERT from another thread moves sqlite3_last_insert_rowid, and lastError_ is
// a plain std::string written from every error path.
//
// So every PUBLIC method locks mu_ for its whole duration. That makes each
// method atomic against the other threads (INSERT + last_insert_rowid,
// INSERT OR IGNORE + UPDATE, the three DELETEs of deleteServer, ...) and gives
// lastError_ a single writer at a time.
//
// mu_ is NOT recursive, so no public method may call another public method.
// Bodies shared between them live in private *Locked() helpers that assume the
// lock is already held (see closeLocked(), the only such case today). Nothing
// under the lock calls back into foreign code, so no lock-order inversion with
// the supervisor/orchestrator locks is possible.
class Db {
 public:
  Db() = default;
  ~Db();

  Db(const Db&) = delete;
  Db& operator=(const Db&) = delete;
  // Not movable: std::mutex is not movable, and the threads above hold a Db&
  // for the object's whole lifetime (AmuWindow::db_, Orchestrator::db_), so
  // relocating one out from under them could never be safe. Verified that
  // nothing in the tree moves a Db - deleting these makes it a compile error.
  Db(Db&&) = delete;
  Db& operator=(Db&&) = delete;

  // Open (or create) the database at `path`. Pass ":memory:" for an in-memory DB.
  // On a fresh DB the four tables are created; on an existing DB the missing
  // columns are added (mirrors _CheckDBColumnAndCreate) and the "-1" global
  // settings row is ensured. Returns false on failure (see lastError()).
  bool open(const std::string& path);
  void close();
  bool isOpen() const;

  // "SELECT * FROM servers JOIN settings ON id = server_id" - every configured
  // server with its settings, ordered by servers.id.
  std::vector<Server> servers();

  // The settings row for `serverId` (default -1 = app-global). `present` is
  // false in the result when the row is absent.
  Settings settings(int64_t serverId = -1);

  // AMU 2.2: per-server pre-shutdown warning plan in the warnplan.h storage
  // format. Writes settings.warnplan; the settings row is created when missing.
  bool saveWarnPlan(int64_t serverId, const std::string& plan);

  // AMU 2.2: scheduled update checks (schedule.h), configured PER SERVER.
  // schedules() returns every row (the watcher walks them all); saveSchedules
  // replaces one server's rows in a transaction (ids > 0 are kept, server_id
  // is forced to `serverId`); markScheduleRun stamps last_run after a firing.
  std::vector<Schedule> schedules();
  bool saveSchedules(int64_t serverId, const std::vector<Schedule>& list);
  bool markScheduleRun(int64_t id, const std::string& stamp);

  // All mods (ordered by modid).
  std::vector<Mod> mods();

  // Insert or replace a mod row keyed by modid (modid is UNIQUE in the schema).
  // Mirrors the updater's "INSERT INTO mods(...)" / "UPDATE mods SET ..." paths.
  // Returns true on success.
  bool upsertMod(const Mod& mod);

  // Append a log line. Mirrors _write_log: date = "YYYY/MM/DD HH:MM:SS", and any
  // single- or double-quoted run inside `entry` is stripped before storing
  // (StringRegExpReplace($text, "([""']).*?\1", "")). Returns the new log_id, or -1.
  int64_t addLog(int64_t serverId, const std::string& type, const std::string& entry);

  // Logs newest-first (ORDER BY log_id DESC), optionally capped (limit<=0 = all).
  std::vector<LogEntry> logs(int limit = 0);

  // Empty the logs table. The AutoIt _ClearLog drops + recreates the table (and
  // VACUUMs); a DELETE + VACUUM is equivalent for consumers - the only visible
  // difference is that log_id keeps counting instead of restarting at 1, which
  // nothing depends on. Returns true on success.
  bool clearLogs();

  // DELETE the cached mods row for `modid` (mirrors _RemoveSelectedMod's
  // "DELETE FROM mods WHERE modid = ..."). Deleting an absent row succeeds.
  bool deleteMod(int64_t modid);

  // Insert a new server (+ its settings row) or update the existing one.
  // If server.id > 0 the matching rows are UPDATEd; otherwise a new server is
  // INSERTed and its generated id returned. Returns the server id, or -1.
  int64_t upsertServer(const Server& server);

  // DELETE the server and its settings + launch rows (amu.au3 line 1190).
  // Returns true on success.
  bool deleteServer(int64_t id);

  // The launch config for `serverId`. When no row exists, returns a default
  // struct with present=false (which builds the ASE/Stopped/BattlEye-on baseline).
  LaunchConfig launch(int64_t serverId);

  // Insert-or-update the launch row for cfg.serverId, stamping updatedAt.
  // Returns true on success.
  bool upsertLaunch(const LaunchConfig& cfg);

  // UPDATE the steamcmd_* columns of the app-global "-1" settings row.
  // `anonymous` is always written; an EMPTY user/pass/guard means "keep the
  // stored value" (that column is skipped - COALESCE-style partial update), so
  // the settings GUI can save without the user re-typing credentials. Values
  // are stored exactly as given - DPAPI encryption is the caller's job
  // (credstore::protect in the binding layer). Returns true on success.
  bool saveGlobalSteamcmd(int anonymous, const std::string& user,
                          const std::string& pass, const std::string& guard);

  // A COPY of the last error message. Returning a reference would hand the UI
  // thread a view into a string the background threads keep reassigning.
  std::string lastError() const;

 private:
  // All private helpers run with mu_ already held (called from a public method).
  void closeLocked();  // close()'s body; open()/~Db() reuse it without relocking
  bool ensureSchema();
  bool tableExists(const char* table);
  bool columnExists(const char* table, const char* column);
  bool addColumnIfMissing(const char* table, const char* column, const char* type);
  bool exec(const char* sql);

  mutable std::mutex mu_;  // guards db_ + lastError_; see the class comment
  sqlite3* db_ = nullptr;
  std::string lastError_;
};

// Strip every quoted run (matched '...' or "...") from `s`, mirroring the AutoIt
// regex "([""']).*?\1". Exposed for unit testing without a database.
std::string stripQuotedRuns(const std::string& s);

// The "DD.MM.YYYY HH:MM:SS" timestamp _write_log stamps (local time; pinned to
// the Swiss/German short-date format the historical amu.db was written with).
std::string nowLogTimestamp();

}  // namespace amucore
