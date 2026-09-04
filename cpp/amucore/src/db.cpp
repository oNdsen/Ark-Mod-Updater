#include "amucore/db.h"

#include <ctime>
#include <mutex>
#include <utility>

#include "sqlite3.h"

namespace amucore {

namespace {

// Bind a std::string as TEXT (SQLITE_TRANSIENT so sqlite copies it).
void bindText(sqlite3_stmt* st, int idx, const std::string& v) {
  sqlite3_bind_text(st, idx, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
}

// Column -> std::string, treating NULL as "" (AutoIt untyped columns read as "").
std::string colText(sqlite3_stmt* st, int col) {
  const unsigned char* p = sqlite3_column_text(st, col);
  if (!p) return {};
  return std::string(reinterpret_cast<const char*>(p),
                     static_cast<size_t>(sqlite3_column_bytes(st, col)));
}

// The settings a NEW server is seeded with (_check_srv_dir, amu.au3 line 2369:
// debug 0, backup 1, force 0, restarttime 5 + the three restart messages).
constexpr const char* kDefaultMsg1 =
    "Mod Update Available! Restart in {minutes} Minutes.";
constexpr const char* kDefaultMsg2 = "Mod Update Available! Restart in 1 Minute.";
constexpr const char* kDefaultMsg3 = "Mod Update Available! Restart Now!";

}  // namespace

// Mirrors StringRegExpReplace($text, "([""']).*?\1", "") - remove each matched
// quoted run (a ' or " up to the next identical quote), non-overlapping, L->R.
std::string stripQuotedRuns(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    const char c = s[i];
    if (c == '\'' || c == '"') {
      const size_t close = s.find(c, i + 1);
      if (close != std::string::npos) {
        i = close + 1;  // drop the whole run including both quotes
        continue;
      }
    }
    out.push_back(c);
    ++i;
  }
  return out;
}

// _DateTimeFormat(_NowCalc(), 2) & " " & _DateTimeFormat(_NowCalc(), 5).
// Format 2 is the REGIONAL short date; the historical amu.db was written on a
// Swiss/German locale, so the on-disk format is "DD.MM.YYYY HH:MM:SS" (verified
// against raw log rows). We pin that format so old and new entries match.
std::string nowLogTimestamp() {
  std::time_t t = std::time(nullptr);
  std::tm lt{};
#if defined(_WIN32)
  localtime_s(&lt, &t);
#else
  localtime_r(&t, &lt);
#endif
  char buf[20];
  std::snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d:%02d", lt.tm_mday,
                lt.tm_mon + 1, lt.tm_year + 1900, lt.tm_hour, lt.tm_min, lt.tm_sec);
  return std::string(buf);
}

Db::~Db() {
  // Take the lock once so a background thread that is still finishing a call
  // has released it before the mutex itself goes away. close() would relock.
  std::lock_guard<std::mutex> lk(mu_);
  closeLocked();
}

void Db::close() {
  std::lock_guard<std::mutex> lk(mu_);
  closeLocked();
}

void Db::closeLocked() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

bool Db::isOpen() const {
  std::lock_guard<std::mutex> lk(mu_);
  return db_ != nullptr;
}

std::string Db::lastError() const {
  std::lock_guard<std::mutex> lk(mu_);
  return lastError_;  // by value - the caller must not alias a racing member
}

bool Db::exec(const char* sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    lastError_ = err ? err : "sqlite exec failed";
    sqlite3_free(err);
    return false;
  }
  return true;
}

bool Db::open(const std::string& path) {
  std::lock_guard<std::mutex> lk(mu_);
  closeLocked();  // NOT close() - mu_ is already held and is not recursive
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    lastError_ = db_ ? sqlite3_errmsg(db_) : "sqlite3_open failed";
    closeLocked();
    return false;
  }
  sqlite3_busy_timeout(db_, 5000);
  return ensureSchema();  // private helper - runs under the lock we hold
}

bool Db::tableExists(const char* table) {
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_,
                         "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;",
                         -1, &st, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_text(st, 1, table, -1, SQLITE_TRANSIENT);
  const bool found = sqlite3_step(st) == SQLITE_ROW;
  sqlite3_finalize(st);
  return found;
}

bool Db::columnExists(const char* table, const char* column) {
  // PRAGMA table_info does not accept a bound parameter for the table name; the
  // table name is a fixed internal literal (never user input), so it is safe here.
  std::string sql = "PRAGMA table_info(";
  sql += table;
  sql += ");";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) return false;
  bool found = false;
  while (sqlite3_step(st) == SQLITE_ROW) {
    if (colText(st, 1) == column) {  // column 1 is the name
      found = true;
      break;
    }
  }
  sqlite3_finalize(st);
  return found;
}

bool Db::addColumnIfMissing(const char* table, const char* column, const char* type) {
  if (columnExists(table, column)) return true;
  std::string sql = "ALTER TABLE ";
  sql += table;
  sql += " ADD COLUMN ";
  sql += column;
  sql += ' ';
  sql += type;
  sql += ';';
  return exec(sql.c_str());
}

bool Db::ensureSchema() {
  // Create tables identically to amu.au3 (lines 175-184).
  if (!tableExists("servers"))
    if (!exec("CREATE TABLE servers (id INTEGER PRIMARY KEY AUTOINCREMENT, name, "
              "path UNIQUE, startscript, map);"))
      return false;
  if (!tableExists("settings"))
    if (!exec("CREATE TABLE settings (server_id INTEGER UNIQUE, debug INTEGER, "
              "backup INTEGER, force INTEGER, restarttime INTEGER, msg1, msg2, msg3);"))
      return false;
  if (!tableExists("mods"))
    if (!exec("CREATE TABLE mods (modid INTEGER UNIQUE, size INTEGER, name TEXT, "
              "usize INTEGER, preview TEXT, olddate TEXT, date TEXT, manifest "
              "INTEGER, timeupdated INTEGER);"))
      return false;
  if (!tableExists("logs"))
    if (!exec("CREATE TABLE logs (log_id INTEGER PRIMARY KEY AUTOINCREMENT, "
              "server_id INTEGER, type, date, entry);"))
      return false;
  // launch: AMU-managed launch config, 1:1 with servers.id (C++ rewrite; replaces
  // the per-server startscript). Additive - the servers.startscript column stays.
  if (!tableExists("launch"))
    if (!exec("CREATE TABLE launch (server_id INTEGER PRIMARY KEY, "
              "game TEXT DEFAULT 'ASE', desired_state INTEGER DEFAULT 0, "
              "auto_restart INTEGER DEFAULT 0, cluster_id TEXT DEFAULT '', "
              "cluster_dir TEXT DEFAULT '', battleye INTEGER DEFAULT 1, "
              "crossplay INTEGER DEFAULT 0, auto_managed INTEGER DEFAULT 0, "
              "extra_args TEXT DEFAULT '', extra_flags TEXT DEFAULT '', "
              "perf_flags TEXT DEFAULT '', max_players INTEGER DEFAULT 0, "
              "updated_at TEXT DEFAULT '', auto_start INTEGER DEFAULT 0);"))
      return false;

  // Add-missing-column pass (amu.au3 lines 152-161).
  addColumnIfMissing("mods", "usize", "INTEGER");
  addColumnIfMissing("mods", "preview", "TEXT");
  addColumnIfMissing("mods", "olddate", "TEXT");
  addColumnIfMissing("mods", "date", "TEXT");
  addColumnIfMissing("mods", "manifest", "INTEGER");
  addColumnIfMissing("mods", "timeupdated", "INTEGER");
  addColumnIfMissing("mods", "posted", "TEXT");  // AMU 2.0: Workshop "Posted" date

  addColumnIfMissing("settings", "steamcmd_anonymous", "INTEGER");
  addColumnIfMissing("settings", "steamcmd_user", "INTEGER");
  addColumnIfMissing("settings", "steamcmd_pass", "INTEGER");
  addColumnIfMissing("settings", "steamcmd_guard", "INTEGER");
  addColumnIfMissing("settings", "warnplan", "TEXT");  // AMU 2.2: pre-shutdown warning plan
  // AMU 2.2: scheduled update checks (schedule.h).
  if (!tableExists("schedules"))
    if (!exec("CREATE TABLE schedules (id INTEGER PRIMARY KEY AUTOINCREMENT, enabled INTEGER, "
              "name TEXT, hour INTEGER, minute INTEGER, days INTEGER, server_id INTEGER, "
              "last_run TEXT);"))
      return false;
  // launch columns: the DEFAULT is smuggled into the type arg (addColumnIfMissing
  // appends it verbatim), so ALTER-added columns on a historical DB carry defaults.
  addColumnIfMissing("launch", "game", "TEXT DEFAULT 'ASE'");
  addColumnIfMissing("launch", "desired_state", "INTEGER DEFAULT 0");
  addColumnIfMissing("launch", "auto_restart", "INTEGER DEFAULT 0");
  addColumnIfMissing("launch", "cluster_id", "TEXT DEFAULT ''");
  addColumnIfMissing("launch", "cluster_dir", "TEXT DEFAULT ''");
  addColumnIfMissing("launch", "battleye", "INTEGER DEFAULT 1");
  addColumnIfMissing("launch", "crossplay", "INTEGER DEFAULT 0");
  addColumnIfMissing("launch", "auto_managed", "INTEGER DEFAULT 0");
  addColumnIfMissing("launch", "extra_args", "TEXT DEFAULT ''");
  addColumnIfMissing("launch", "extra_flags", "TEXT DEFAULT ''");
  addColumnIfMissing("launch", "perf_flags", "TEXT DEFAULT ''");
  addColumnIfMissing("launch", "max_players", "INTEGER DEFAULT 0");
  addColumnIfMissing("launch", "updated_at", "TEXT DEFAULT ''");
  addColumnIfMissing("launch", "auto_start", "INTEGER DEFAULT 0");

  // Ensure the app-global "-1" settings row exists (amu.au3 line 163).
  {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(
            db_, "INSERT OR IGNORE INTO settings(server_id, msg1) VALUES (-1, ?);", -1,
            &st, nullptr) == SQLITE_OK) {
      bindText(st, 1, nowLogTimestamp());
      sqlite3_step(st);
      sqlite3_finalize(st);
    }
  }
  return true;
}

std::vector<Server> Db::servers() {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<Server> out;
  sqlite3_stmt* st = nullptr;
  // Explicit column list (not SELECT *) so struct mapping is order-independent
  // of on-disk column order, while matching the AutoIt JOIN semantics.
  const char* sql =
      "SELECT s.id, s.name, s.path, s.startscript, s.map, "
      "t.server_id, t.debug, t.backup, t.force, t.restarttime, "
      "t.msg1, t.msg2, t.msg3, t.warnplan "
      "FROM servers s JOIN settings t ON s.id = t.server_id ORDER BY s.id;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return out;
  }
  while (sqlite3_step(st) == SQLITE_ROW) {
    Server sv;
    sv.id = sqlite3_column_int64(st, 0);
    sv.name = colText(st, 1);
    sv.path = colText(st, 2);
    sv.startscript = colText(st, 3);
    sv.map = colText(st, 4);
    sv.serverId = sqlite3_column_int64(st, 5);
    sv.debug = sqlite3_column_int(st, 6);
    sv.backup = sqlite3_column_int(st, 7);
    sv.force = sqlite3_column_int(st, 8);
    sv.restarttime = sqlite3_column_int(st, 9);
    sv.msg1 = colText(st, 10);
    sv.msg2 = colText(st, 11);
    sv.msg3 = colText(st, 12);
    sv.warnplan = colText(st, 13);
    out.push_back(std::move(sv));
  }
  sqlite3_finalize(st);
  return out;
}

Settings Db::settings(int64_t serverId) {
  std::lock_guard<std::mutex> lk(mu_);
  Settings out;
  out.serverId = serverId;
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "SELECT server_id, debug, backup, force, restarttime, msg1, msg2, msg3, "
      "steamcmd_anonymous, steamcmd_user, steamcmd_pass, steamcmd_guard, warnplan "
      "FROM settings WHERE server_id = ?;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return out;
  }
  sqlite3_bind_int64(st, 1, serverId);
  if (sqlite3_step(st) == SQLITE_ROW) {
    out.present = true;
    out.serverId = sqlite3_column_int64(st, 0);
    out.debug = sqlite3_column_int(st, 1);
    out.backup = sqlite3_column_int(st, 2);
    out.force = sqlite3_column_int(st, 3);
    out.restarttime = sqlite3_column_int(st, 4);
    out.msg1 = colText(st, 5);
    out.msg2 = colText(st, 6);
    out.msg3 = colText(st, 7);
    out.steamcmdAnonymous = sqlite3_column_int(st, 8);
    // DPAPI-encrypted strings ("DPAPI:0x...") despite the INTEGER type affinity.
    out.steamcmdUser = colText(st, 9);
    out.steamcmdPass = colText(st, 10);
    out.steamcmdGuard = colText(st, 11);
    out.warnplan = colText(st, 12);
  }
  sqlite3_finalize(st);
  return out;
}

std::vector<Mod> Db::mods() {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<Mod> out;
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "SELECT modid, size, name, usize, preview, olddate, date, manifest, "
      "timeupdated, posted FROM mods ORDER BY modid;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return out;
  }
  while (sqlite3_step(st) == SQLITE_ROW) {
    Mod m;
    m.modid = sqlite3_column_int64(st, 0);
    m.size = sqlite3_column_int64(st, 1);
    m.name = colText(st, 2);
    m.usize = sqlite3_column_int64(st, 3);
    m.preview = colText(st, 4);
    m.olddate = colText(st, 5);
    m.date = colText(st, 6);
    m.manifest = sqlite3_column_int64(st, 7);
    m.timeupdated = sqlite3_column_int64(st, 8);
    m.posted = colText(st, 9);
    out.push_back(std::move(m));
  }
  sqlite3_finalize(st);
  return out;
}

bool Db::upsertMod(const Mod& m) {
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "INSERT OR REPLACE INTO mods(modid, size, name, usize, preview, olddate, "
      "date, manifest, timeupdated, posted) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, m.modid);
  sqlite3_bind_int64(st, 2, m.size);
  bindText(st, 3, m.name);
  sqlite3_bind_int64(st, 4, m.usize);
  bindText(st, 5, m.preview);
  bindText(st, 6, m.olddate);
  bindText(st, 7, m.date);
  sqlite3_bind_int64(st, 8, m.manifest);
  sqlite3_bind_int64(st, 9, m.timeupdated);
  bindText(st, 10, m.posted);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}

int64_t Db::addLog(int64_t serverId, const std::string& type, const std::string& entry) {
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "INSERT INTO logs(server_id, type, date, entry) VALUES (?, ?, ?, ?);";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return -1;
  }
  sqlite3_bind_int64(st, 1, serverId);
  bindText(st, 2, type);
  bindText(st, 3, nowLogTimestamp());
  bindText(st, 4, stripQuotedRuns(entry));  // mirror _write_log's quote strip
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) {
    lastError_ = sqlite3_errmsg(db_);
    return -1;
  }
  // Safe under mu_: last_insert_rowid is per CONNECTION, so it would report
  // another thread's INSERT (into servers/settings/...) if one could slip in
  // between the step above and this call.
  return sqlite3_last_insert_rowid(db_);
}

std::vector<LogEntry> Db::logs(int limit) {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<LogEntry> out;
  sqlite3_stmt* st = nullptr;
  const char* sqlAll =
      "SELECT log_id, server_id, type, date, entry FROM logs ORDER BY log_id DESC;";
  const char* sqlLim =
      "SELECT log_id, server_id, type, date, entry FROM logs ORDER BY log_id DESC "
      "LIMIT ?;";
  if (sqlite3_prepare_v2(db_, limit > 0 ? sqlLim : sqlAll, -1, &st, nullptr) !=
      SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return out;
  }
  if (limit > 0) sqlite3_bind_int(st, 1, limit);
  while (sqlite3_step(st) == SQLITE_ROW) {
    LogEntry e;
    e.logId = sqlite3_column_int64(st, 0);
    e.serverId = sqlite3_column_int64(st, 1);
    e.type = colText(st, 2);
    e.date = colText(st, 3);
    e.entry = colText(st, 4);
    out.push_back(std::move(e));
  }
  sqlite3_finalize(st);
  return out;
}

bool Db::clearLogs() {
  std::lock_guard<std::mutex> lk(mu_);
  // _ClearLog does DROP TABLE + CREATE TABLE + VACUUM; DELETE + VACUUM keeps
  // the same schema object alive (only log_id numbering continues) and cannot
  // race prepared statements against a dropped table.
  if (!exec("DELETE FROM logs;")) return false;
  exec("VACUUM;");  // shrink like the AutoIt; best-effort
  return true;
}

bool Db::deleteMod(int64_t modid) {
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, "DELETE FROM mods WHERE modid=?;", -1, &st, nullptr) !=
      SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, modid);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}

int64_t Db::upsertServer(const Server& sv) {
  // The whole INSERT/UPDATE sequence runs under one lock: the servers INSERT and
  // the sqlite3_last_insert_rowid that reads its id must not be separated by any
  // other thread's INSERT (the background log sink writes to logs continuously),
  // or the settings + launch rows below would be keyed on a logs rowid and the
  // new server would never show up in servers() (INNER JOIN settings).
  std::lock_guard<std::mutex> lk(mu_);
  if (sv.id > 0) {
    // UPDATE servers + UPDATE settings (amu.au3 lines 1146-1156).
    {
      sqlite3_stmt* st = nullptr;
      const char* sql =
          "UPDATE servers SET name=?, path=?, startscript=?, map=? WHERE id=?;";
      if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
      }
      bindText(st, 1, sv.name);
      bindText(st, 2, sv.path);
      bindText(st, 3, sv.startscript);
      bindText(st, 4, sv.map);
      sqlite3_bind_int64(st, 5, sv.id);
      const bool ok = sqlite3_step(st) == SQLITE_DONE;
      sqlite3_finalize(st);
      if (!ok) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
      }
    }
    {
      // Ensure a settings row exists, then update it (INSERT OR IGNORE keeps the
      // per-server row present without disturbing existing values).
      sqlite3_stmt* si = nullptr;
      if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO settings(server_id) VALUES(?);",
                             -1, &si, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(si, 1, sv.id);
        sqlite3_step(si);
        sqlite3_finalize(si);
      }
      sqlite3_stmt* st = nullptr;
      const char* sql =
          "UPDATE settings SET debug=?, backup=?, force=?, restarttime=?, "
          "msg1=?, msg2=?, msg3=? WHERE server_id=?;";
      if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
      }
      sqlite3_bind_int(st, 1, sv.debug);
      sqlite3_bind_int(st, 2, sv.backup);
      sqlite3_bind_int(st, 3, sv.force);
      sqlite3_bind_int(st, 4, sv.restarttime);
      bindText(st, 5, sv.msg1);
      bindText(st, 6, sv.msg2);
      bindText(st, 7, sv.msg3);
      sqlite3_bind_int64(st, 8, sv.id);
      const bool ok = sqlite3_step(st) == SQLITE_DONE;
      sqlite3_finalize(st);
      if (!ok) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
      }
    }
    {
      // Keep a launch row co-existing (defaults) so launch config is always joinable.
      sqlite3_stmt* sl = nullptr;
      if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO launch(server_id) VALUES(?);", -1,
                             &sl, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(sl, 1, sv.id);
        sqlite3_step(sl);
        sqlite3_finalize(sl);
      }
    }
    return sv.id;
  }

  // INSERT a new server, then its settings row.
  int64_t newId = -1;
  {
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "INSERT INTO servers(name, path, startscript, map) VALUES(?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
      lastError_ = sqlite3_errmsg(db_);
      return -1;
    }
    bindText(st, 1, sv.name);
    bindText(st, 2, sv.path);
    bindText(st, 3, sv.startscript);
    bindText(st, 4, sv.map);
    const bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    if (!ok) {
      lastError_ = sqlite3_errmsg(db_);
      return -1;
    }
    newId = sqlite3_last_insert_rowid(db_);
  }
  {
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "INSERT INTO settings(server_id, debug, backup, force, restarttime, "
        "msg1, msg2, msg3) VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
      lastError_ = sqlite3_errmsg(db_);
      return -1;
    }
    // Seed the AutoIt new-server defaults wherever the caller left the struct
    // default - explicit values still win. `backup`/`restarttime` cannot
    // distinguish an explicit 0 from the struct default, so a NEW server always
    // starts with backup=1 / restarttime=5 (exactly what _check_srv_dir seeds);
    // use the UPDATE path afterwards to disable. Does NOT affect existing rows.
    sqlite3_bind_int64(st, 1, newId);
    sqlite3_bind_int(st, 2, sv.debug);
    sqlite3_bind_int(st, 3, sv.backup != 0 ? sv.backup : 1);
    sqlite3_bind_int(st, 4, sv.force);
    sqlite3_bind_int(st, 5, sv.restarttime > 0 ? sv.restarttime : 5);
    bindText(st, 6, sv.msg1.empty() ? std::string(kDefaultMsg1) : sv.msg1);
    bindText(st, 7, sv.msg2.empty() ? std::string(kDefaultMsg2) : sv.msg2);
    bindText(st, 8, sv.msg3.empty() ? std::string(kDefaultMsg3) : sv.msg3);
    const bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    if (!ok) {
      lastError_ = sqlite3_errmsg(db_);
      return -1;
    }
  }
  {
    // Co-existing launch row (defaults) for the new server.
    sqlite3_stmt* sl = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO launch(server_id) VALUES(?);", -1,
                           &sl, nullptr) == SQLITE_OK) {
      sqlite3_bind_int64(sl, 1, newId);
      sqlite3_step(sl);
      sqlite3_finalize(sl);
    }
  }
  return newId;
}

bool Db::deleteServer(int64_t id) {
  // One lock for all three DELETEs so no thread ever observes (or writes into)
  // a server whose settings/launch rows are half gone.
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, "DELETE FROM servers WHERE id=?;", -1, &st, nullptr) !=
      SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, id);
  bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  if (sqlite3_prepare_v2(db_, "DELETE FROM settings WHERE server_id=?;", -1, &st,
                         nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, id);
  ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  if (sqlite3_prepare_v2(db_, "DELETE FROM launch WHERE server_id=?;", -1, &st,
                         nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, id);
  ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}

LaunchConfig Db::launch(int64_t serverId) {
  std::lock_guard<std::mutex> lk(mu_);
  LaunchConfig out;
  out.serverId = serverId;
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "SELECT server_id, game, desired_state, auto_restart, cluster_id, cluster_dir, "
      "battleye, crossplay, auto_managed, extra_args, extra_flags, perf_flags, "
      "max_players, updated_at, auto_start FROM launch WHERE server_id = ?;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return out;
  }
  sqlite3_bind_int64(st, 1, serverId);
  if (sqlite3_step(st) == SQLITE_ROW) {
    out.present = true;
    out.serverId = sqlite3_column_int64(st, 0);
    out.game = colText(st, 1);
    out.desiredState = sqlite3_column_int(st, 2);
    out.autoRestart = sqlite3_column_int(st, 3);
    out.clusterId = colText(st, 4);
    out.clusterDir = colText(st, 5);
    out.battleye = sqlite3_column_int(st, 6);
    out.crossplay = sqlite3_column_int(st, 7);
    out.autoManaged = sqlite3_column_int(st, 8);
    out.extraArgs = colText(st, 9);
    out.extraFlags = colText(st, 10);
    out.perfFlags = colText(st, 11);
    out.maxPlayers = sqlite3_column_int(st, 12);
    out.updatedAt = colText(st, 13);
    out.autoStart = sqlite3_column_int(st, 14);
    if (out.game.empty()) out.game = "ASE";
  }
  sqlite3_finalize(st);
  return out;
}

bool Db::upsertLaunch(const LaunchConfig& cfg) {
  std::lock_guard<std::mutex> lk(mu_);
  // Ensure the row exists (INSERT OR IGNORE), then UPDATE it - mirrors the
  // settings upsert pattern so it works whether or not a row is present.
  {
    sqlite3_stmt* si = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO launch(server_id) VALUES(?);", -1,
                           &si, nullptr) == SQLITE_OK) {
      sqlite3_bind_int64(si, 1, cfg.serverId);
      sqlite3_step(si);
      sqlite3_finalize(si);
    }
  }
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "UPDATE launch SET game=?, desired_state=?, auto_restart=?, cluster_id=?, "
      "cluster_dir=?, battleye=?, crossplay=?, auto_managed=?, extra_args=?, "
      "extra_flags=?, perf_flags=?, max_players=?, updated_at=?, auto_start=? "
      "WHERE server_id=?;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  bindText(st, 1, cfg.game.empty() ? "ASE" : cfg.game);
  sqlite3_bind_int(st, 2, cfg.desiredState);
  sqlite3_bind_int(st, 3, cfg.autoRestart);
  bindText(st, 4, cfg.clusterId);
  bindText(st, 5, cfg.clusterDir);
  sqlite3_bind_int(st, 6, cfg.battleye);
  sqlite3_bind_int(st, 7, cfg.crossplay);
  sqlite3_bind_int(st, 8, cfg.autoManaged);
  bindText(st, 9, cfg.extraArgs);
  bindText(st, 10, cfg.extraFlags);
  bindText(st, 11, cfg.perfFlags);
  sqlite3_bind_int(st, 12, cfg.maxPlayers);
  bindText(st, 13, nowLogTimestamp());
  sqlite3_bind_int(st, 14, cfg.autoStart);
  sqlite3_bind_int64(st, 15, cfg.serverId);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}

bool Db::saveGlobalSteamcmd(int anonymous, const std::string& user,
                            const std::string& pass, const std::string& guard) {
  std::lock_guard<std::mutex> lk(mu_);
  // Dynamic SET list: only the non-empty credentials are written. The SQL text
  // varies only in fixed internal column names - every VALUE stays a bound
  // parameter (never concatenated).
  std::string sql = "UPDATE settings SET steamcmd_anonymous=?";
  if (!user.empty()) sql += ", steamcmd_user=?";
  if (!pass.empty()) sql += ", steamcmd_pass=?";
  if (!guard.empty()) sql += ", steamcmd_guard=?";
  sql += " WHERE server_id=-1;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  int idx = 1;
  sqlite3_bind_int(st, idx++, anonymous);
  if (!user.empty()) bindText(st, idx++, user);
  if (!pass.empty()) bindText(st, idx++, pass);
  if (!guard.empty()) bindText(st, idx++, guard);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}


bool Db::saveWarnPlan(int64_t serverId, const std::string& plan) {
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  // The settings row normally exists (upsertServer seeds it); make sure anyway.
  if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO settings(server_id) VALUES(?);", -1, &st,
                         nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, serverId);
  sqlite3_step(st);
  sqlite3_finalize(st);
  if (sqlite3_prepare_v2(db_, "UPDATE settings SET warnplan = ? WHERE server_id = ?;", -1, &st,
                         nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  bindText(st, 1, plan);
  sqlite3_bind_int64(st, 2, serverId);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}

std::vector<Schedule> Db::schedules() {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<Schedule> out;
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "SELECT id, enabled, name, hour, minute, days, server_id, last_run "
      "FROM schedules ORDER BY id;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return out;
  }
  while (sqlite3_step(st) == SQLITE_ROW) {
    Schedule s;
    s.id = sqlite3_column_int64(st, 0);
    s.enabled = sqlite3_column_int(st, 1) != 0;
    s.name = colText(st, 2);
    s.hour = sqlite3_column_int(st, 3);
    s.minute = sqlite3_column_int(st, 4);
    s.days = sqlite3_column_int(st, 5);
    s.serverId = sqlite3_column_int64(st, 6);
    s.lastRun = colText(st, 7);
    out.push_back(std::move(s));
  }
  sqlite3_finalize(st);
  return out;
}

bool Db::saveSchedules(int64_t serverId, const std::vector<Schedule>& list) {
  std::lock_guard<std::mutex> lk(mu_);
  if (sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  // last_run is owned by the watcher, not by the UI: the list the UI sends was
  // captured when the tab opened and may carry a stale stamp, which would let
  // the once-per-minute guard fire a second run. Keep the stored stamps by id.
  std::vector<std::pair<int64_t, std::string>> lastRuns;
  {
    sqlite3_stmt* q = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT id, last_run FROM schedules WHERE server_id = ?;", -1,
                           &q, nullptr) == SQLITE_OK) {
      sqlite3_bind_int64(q, 1, serverId);
      while (sqlite3_step(q) == SQLITE_ROW)
        lastRuns.emplace_back(sqlite3_column_int64(q, 0), colText(q, 1));
      sqlite3_finalize(q);
    }
  }
  sqlite3_stmt* st = nullptr;
  bool ok = sqlite3_prepare_v2(db_, "DELETE FROM schedules WHERE server_id = ?;", -1, &st,
                               nullptr) == SQLITE_OK;
  if (ok) {
    sqlite3_bind_int64(st, 1, serverId);
    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    st = nullptr;
  }
  if (ok && sqlite3_prepare_v2(db_,
                               "INSERT INTO schedules(id, enabled, name, hour, minute, days, "
                               "server_id, last_run) VALUES(?, ?, ?, ?, ?, ?, ?, ?);",
                               -1, &st, nullptr) != SQLITE_OK) {
    ok = false;
  }
  for (size_t i = 0; ok && i < list.size(); ++i) {
    const Schedule& s = list[i];
    sqlite3_reset(st);
    sqlite3_clear_bindings(st);
    if (s.id > 0) sqlite3_bind_int64(st, 1, s.id); else sqlite3_bind_null(st, 1);
    sqlite3_bind_int(st, 2, s.enabled ? 1 : 0);
    bindText(st, 3, s.name);
    sqlite3_bind_int(st, 4, s.hour);
    sqlite3_bind_int(st, 5, s.minute);
    sqlite3_bind_int(st, 6, s.days);
    sqlite3_bind_int64(st, 7, serverId);  // always this server's rows
    std::string lastRun;  // new rows start empty; existing ids keep the stored stamp
    for (const auto& p : lastRuns)
      if (s.id > 0 && p.first == s.id) lastRun = p.second;
    bindText(st, 8, lastRun);
    ok = sqlite3_step(st) == SQLITE_DONE;
  }
  if (st) sqlite3_finalize(st);
  if (!ok) {
    lastError_ = sqlite3_errmsg(db_);
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    return false;
  }
  if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    return false;
  }
  return true;
}

bool Db::markScheduleRun(int64_t id, const std::string& stamp) {
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, "UPDATE schedules SET last_run = ? WHERE id = ?;", -1, &st,
                         nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  bindText(st, 1, stamp);
  sqlite3_bind_int64(st, 2, id);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}


bool Db::updateModInstall(const Mod& m) {
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO mods(modid, size) VALUES(?, 0);", -1, &st,
                         nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, m.modid);
  sqlite3_step(st);
  sqlite3_finalize(st);
  const char* sql =
      "UPDATE mods SET size = ?, usize = ?, timeupdated = ?, manifest = ?, "
      "name = CASE WHEN ? = '' THEN name ELSE ? END, "
      "preview = CASE WHEN ? = '' THEN preview ELSE ? END WHERE modid = ?;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, m.size);
  sqlite3_bind_int64(st, 2, m.usize);
  sqlite3_bind_int64(st, 3, m.timeupdated);
  sqlite3_bind_int64(st, 4, m.manifest);
  bindText(st, 5, m.name);
  bindText(st, 6, m.name);
  bindText(st, 7, m.preview);
  bindText(st, 8, m.preview);
  sqlite3_bind_int64(st, 9, m.modid);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}

bool Db::updateModMeta(int64_t modid, const std::string& name, const std::string& preview,
                       const std::string& date, const std::string& posted) {
  std::lock_guard<std::mutex> lk(mu_);
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO mods(modid, size) VALUES(?, 0);", -1, &st,
                         nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  sqlite3_bind_int64(st, 1, modid);
  sqlite3_step(st);
  sqlite3_finalize(st);
  const char* sql =
      "UPDATE mods SET "
      "name = CASE WHEN ? = '' THEN name ELSE ? END, "
      "preview = CASE WHEN ? = '' THEN preview ELSE ? END, "
      "date = CASE WHEN ? = '' THEN date ELSE ? END, "
      "posted = CASE WHEN ? = '' THEN posted ELSE ? END WHERE modid = ?;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
    lastError_ = sqlite3_errmsg(db_);
    return false;
  }
  bindText(st, 1, name);    bindText(st, 2, name);
  bindText(st, 3, preview); bindText(st, 4, preview);
  bindText(st, 5, date);    bindText(st, 6, date);
  bindText(st, 7, posted);  bindText(st, 8, posted);
  sqlite3_bind_int64(st, 9, modid);
  const bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  if (!ok) lastError_ = sqlite3_errmsg(db_);
  return ok;
}

}  // namespace amucore
