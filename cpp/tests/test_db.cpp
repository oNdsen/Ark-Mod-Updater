#include <doctest/doctest.h>

#include <type_traits>

#include "amucore/db.h"

using namespace amucore;

TEST_CASE("open :memory: creates the schema and the -1 global settings row") {
  Db db;
  REQUIRE(db.open(":memory:"));

  // The app-global settings row (server_id == -1) is auto-created on open.
  const Settings g = db.settings(-1);
  CHECK(g.present);
  CHECK(g.serverId == -1);
  CHECK_FALSE(g.msg1.empty());  // seeded with the current timestamp
}

TEST_CASE("insert a server + settings, read it back via the servers() JOIN") {
  Db db;
  REQUIRE(db.open(":memory:"));

  Server sv;
  sv.name = "Main";
  sv.path = "C:\\ARK\\Server1";
  sv.startscript = "C:\\ARK\\Server1\\start.bat";
  sv.map = "TheIsland";
  sv.debug = 1;
  sv.backup = 1;
  sv.force = 0;
  sv.restarttime = 3600;
  sv.msg1 = "Update in 10 min";
  sv.msg2 = "Update in 1 min";
  sv.msg3 = "Restarting now";

  const int64_t id = db.upsertServer(sv);
  REQUIRE(id > 0);

  const auto rows = db.servers();
  REQUIRE(rows.size() == 1);
  const Server& r = rows[0];
  CHECK(r.id == id);
  CHECK(r.serverId == id);  // the JOIN key mirrors the $eSERVERID enum slot
  CHECK(r.name == "Main");
  CHECK(r.path == "C:\\ARK\\Server1");
  CHECK(r.startscript == "C:\\ARK\\Server1\\start.bat");
  CHECK(r.map == "TheIsland");
  CHECK(r.debug == 1);
  CHECK(r.backup == 1);
  CHECK(r.force == 0);
  CHECK(r.restarttime == 3600);
  CHECK(r.msg1 == "Update in 10 min");
  CHECK(r.msg2 == "Update in 1 min");
  CHECK(r.msg3 == "Restarting now");
}

TEST_CASE("update an existing server preserves the row and changes fields") {
  Db db;
  REQUIRE(db.open(":memory:"));

  Server sv;
  sv.name = "Old";
  sv.path = "C:\\ARK\\S";
  sv.map = "TheIsland";
  const int64_t id = db.upsertServer(sv);
  REQUIRE(id > 0);

  Server upd = sv;
  upd.id = id;
  upd.name = "New";
  upd.map = "Ragnarok";
  upd.force = 1;
  upd.msg1 = "hello";
  const int64_t id2 = db.upsertServer(upd);
  CHECK(id2 == id);

  const auto rows = db.servers();
  REQUIRE(rows.size() == 1);  // still exactly one server
  CHECK(rows[0].name == "New");
  CHECK(rows[0].map == "Ragnarok");
  CHECK(rows[0].force == 1);
  CHECK(rows[0].msg1 == "hello");
}

TEST_CASE("mods round-trip: upsertMod then read back via mods()") {
  Db db;
  REQUIRE(db.open(":memory:"));
  CHECK(db.mods().empty());  // fresh schema

  Mod a;
  a.modid = 632091170;
  a.size = 12345;
  a.name = "Structures Plus";
  a.usize = 54321;
  a.preview = "http://img/preview.jpg";
  a.date = "2026/07/03 10:00:00";
  a.manifest = 999;
  a.timeupdated = 1720000000;
  REQUIRE(db.upsertMod(a));

  Mod b;
  b.modid = 3000000000;  // > 2^31, must survive as int64
  b.size = 1;
  b.name = "Big Id Mod";
  REQUIRE(db.upsertMod(b));

  auto rows = db.mods();
  REQUIRE(rows.size() == 2);
  // ORDER BY modid ascending.
  CHECK(rows[0].modid == 632091170);
  CHECK(rows[0].name == "Structures Plus");
  CHECK(rows[0].size == 12345);
  CHECK(rows[0].usize == 54321);
  CHECK(rows[0].preview == "http://img/preview.jpg");
  CHECK(rows[0].manifest == 999);
  CHECK(rows[0].timeupdated == 1720000000);
  CHECK(rows[1].modid == 3000000000);

  // modid is UNIQUE - a second upsert replaces the row, not duplicates it.
  Mod a2 = a;
  a2.name = "Structures Plus (renamed)";
  REQUIRE(db.upsertMod(a2));
  rows = db.mods();
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].name == "Structures Plus (renamed)");
}

TEST_CASE("addLog strips quoted runs and stores a timestamped entry") {
  Db db;
  REQUIRE(db.open(":memory:"));

  // Mirrors _write_log: the "'...'" and "\"...\"" runs are removed.
  const int64_t logId =
      db.addLog(5, "normal", "Removed mod 'SuperMod' from server \"Main\" ok");
  REQUIRE(logId > 0);

  const auto rows = db.logs(0);
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].serverId == 5);
  CHECK(rows[0].type == "normal");
  CHECK(rows[0].entry == "Removed mod  from server  ok");
  // "DD.MM.YYYY HH:MM:SS" == 19 chars (historical Swiss short-date format).
  CHECK(rows[0].date.size() == 19);
  CHECK(rows[0].date[2] == '.');
  CHECK(rows[0].date[5] == '.');
  CHECK(rows[0].date[10] == ' ');
  CHECK(rows[0].date[13] == ':');
}

TEST_CASE("logs newest-first and limit") {
  Db db;
  REQUIRE(db.open(":memory:"));
  CHECK(db.addLog(1, "normal", "first") > 0);
  CHECK(db.addLog(1, "normal", "second") > 0);
  CHECK(db.addLog(1, "normal", "third") > 0);

  const auto all = db.logs(0);
  REQUIRE(all.size() == 3);
  CHECK(all[0].entry == "third");  // DESC by log_id
  CHECK(all[2].entry == "first");

  const auto lim = db.logs(2);
  REQUIRE(lim.size() == 2);
  CHECK(lim[0].entry == "third");
  CHECK(lim[1].entry == "second");
}

TEST_CASE("deleteServer removes the server and its settings row") {
  Db db;
  REQUIRE(db.open(":memory:"));

  Server a;
  a.name = "A";
  a.path = "C:\\ARK\\A";
  Server b;
  b.name = "B";
  b.path = "C:\\ARK\\B";
  const int64_t idA = db.upsertServer(a);
  const int64_t idB = db.upsertServer(b);
  REQUIRE(idA > 0);
  REQUIRE(idB > 0);
  REQUIRE(db.servers().size() == 2);

  CHECK(db.deleteServer(idA));
  const auto rows = db.servers();
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].id == idB);

  // The per-server settings row is gone too; the -1 global row remains.
  CHECK_FALSE(db.settings(idA).present);
  CHECK(db.settings(-1).present);
}

TEST_CASE("launch config: absent row yields defaults, upsert round-trips") {
  Db db;
  REQUIRE(db.open(":memory:"));

  Server sv;
  sv.name = "S";
  sv.path = "C:\\ARK\\S";
  const int64_t id = db.upsertServer(sv);
  REQUIRE(id > 0);

  // upsertServer seeds a co-existing launch row, but it holds all-defaults.
  LaunchConfig def = db.launch(id);
  CHECK(def.present);            // row exists (INSERT OR IGNORE in upsertServer)
  CHECK(def.game == "ASE");
  CHECK(def.desiredState == 0);
  CHECK(def.autoRestart == 0);
  CHECK(def.battleye == 1);      // BattlEye on by default (ARK default)
  CHECK(def.maxPlayers == 0);    // 0 == omit / defer to ini

  // A server that never got a launch row still reads sensible defaults.
  LaunchConfig none = db.launch(999999);
  CHECK_FALSE(none.present);
  CHECK(none.game == "ASE");
  CHECK(none.battleye == 1);

  // Round-trip a full config.
  LaunchConfig cfg;
  cfg.serverId = id;
  cfg.game = "ASA";
  cfg.desiredState = 1;
  cfg.autoRestart = 1;
  cfg.clusterId = "myc";
  cfg.clusterDir = "D:\\Cluster";
  cfg.battleye = 0;
  cfg.crossplay = 1;
  cfg.autoManaged = 0;
  cfg.extraArgs = "-foo -bar";
  cfg.extraFlags = "-USEALLAVAILABLECORES";
  cfg.perfFlags = "-nomemorybias";
  cfg.maxPlayers = 50;
  REQUIRE(db.upsertLaunch(cfg));

  LaunchConfig r = db.launch(id);
  CHECK(r.present);
  CHECK(r.game == "ASA");
  CHECK(r.desiredState == 1);
  CHECK(r.autoRestart == 1);
  CHECK(r.clusterId == "myc");
  CHECK(r.clusterDir == "D:\\Cluster");
  CHECK(r.battleye == 0);
  CHECK(r.crossplay == 1);
  CHECK(r.extraArgs == "-foo -bar");
  CHECK(r.extraFlags == "-USEALLAVAILABLECORES");
  CHECK(r.perfFlags == "-nomemorybias");
  CHECK(r.maxPlayers == 50);
  CHECK(r.updatedAt.size() == 19);  // stamped "YYYY/MM/DD HH:MM:SS"

  // Second upsert updates in place (still one row).
  cfg.game = "ASE";
  cfg.maxPlayers = 30;
  REQUIRE(db.upsertLaunch(cfg));
  LaunchConfig r2 = db.launch(id);
  CHECK(r2.game == "ASE");
  CHECK(r2.maxPlayers == 30);
}

TEST_CASE("deleteServer also clears the launch row") {
  Db db;
  REQUIRE(db.open(":memory:"));
  Server sv;
  sv.name = "S";
  sv.path = "C:\\ARK\\S";
  const int64_t id = db.upsertServer(sv);
  REQUIRE(id > 0);

  LaunchConfig cfg;
  cfg.serverId = id;
  cfg.game = "ASA";
  REQUIRE(db.upsertLaunch(cfg));
  CHECK(db.launch(id).present);

  CHECK(db.deleteServer(id));
  CHECK_FALSE(db.launch(id).present);  // launch row gone with the server
}

TEST_CASE("clearLogs empties the logs table and logging keeps working") {
  Db db;
  REQUIRE(db.open(":memory:"));
  CHECK(db.addLog(1, "normal", "one") > 0);
  CHECK(db.addLog(2, "debug", "two") > 0);
  REQUIRE(db.logs(0).size() == 2);

  CHECK(db.clearLogs());
  CHECK(db.logs(0).empty());

  // The table object survives (DELETE, not the AutoIt DROP+CREATE) - the next
  // insert works and lands as the only row.
  CHECK(db.addLog(0, "normal", "Logfile Deleted.") > 0);
  const auto rows = db.logs(0);
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].entry == "Logfile Deleted.");

  // Clearing an already-empty table succeeds too.
  CHECK(db.clearLogs());
  CHECK(db.logs(0).empty());
}

TEST_CASE("deleteMod removes only the matching cache row") {
  Db db;
  REQUIRE(db.open(":memory:"));

  Mod a;
  a.modid = 111;
  a.name = "Keep";
  Mod b;
  b.modid = 3000000000;  // > 2^31 - the id must bind as int64
  b.name = "Drop";
  REQUIRE(db.upsertMod(a));
  REQUIRE(db.upsertMod(b));
  REQUIRE(db.mods().size() == 2);

  CHECK(db.deleteMod(3000000000));
  const auto rows = db.mods();
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].modid == 111);

  // Deleting an absent row is a no-op success (mirrors the AutoIt DELETE).
  CHECK(db.deleteMod(999999));
  CHECK(db.mods().size() == 1);
}

TEST_CASE("settings reads the steamcmd credential columns as text") {
  Db db;
  REQUIRE(db.open(":memory:"));

  // The AutoIt stores DPAPI-encrypted STRINGS ("DPAPI:0x...") in the INTEGER-
  // affinity steamcmd_* columns; the struct fields are std::string so nothing
  // gets truncated to an int. On a fresh -1 row the columns are NULL -> "".
  const Settings g = db.settings(-1);
  REQUIRE(g.present);
  CHECK(g.steamcmdAnonymous == 0);
  CHECK(g.steamcmdUser.empty());
  CHECK(g.steamcmdPass.empty());
  CHECK(g.steamcmdGuard.empty());
  static_assert(std::is_same_v<decltype(Settings::steamcmdUser), std::string> &&
                    std::is_same_v<decltype(Settings::steamcmdPass), std::string> &&
                    std::is_same_v<decltype(Settings::steamcmdGuard), std::string>,
                "steamcmd credentials must be strings (DPAPI blobs)");
}

TEST_CASE("a new server is seeded with the AutoIt settings defaults") {
  Db db;
  REQUIRE(db.open(":memory:"));

  // Only name/path/map set - the settings block stays at struct defaults, so
  // the INSERT must seed what _check_srv_dir seeds (amu.au3 line 2369).
  Server sv;
  sv.name = "Fresh";
  sv.path = "C:\\ARK\\Fresh";
  sv.map = "TheIsland";
  const int64_t id = db.upsertServer(sv);
  REQUIRE(id > 0);

  auto rows = db.servers();
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].debug == 0);
  CHECK(rows[0].backup == 1);
  CHECK(rows[0].force == 0);
  CHECK(rows[0].restarttime == 5);
  CHECK(rows[0].msg1 == "Mod Update Available! Restart in {minutes} Minutes.");
  CHECK(rows[0].msg2 == "Mod Update Available! Restart in 1 Minute.");
  CHECK(rows[0].msg3 == "Mod Update Available! Restart Now!");

  // Explicit values on INSERT still win over the seeds (per-field).
  Server cust;
  cust.name = "Custom";
  cust.path = "C:\\ARK\\Custom";
  cust.restarttime = 10;
  cust.msg1 = "own msg";
  REQUIRE(db.upsertServer(cust) > 0);
  rows = db.servers();
  REQUIRE(rows.size() == 2);
  CHECK(rows[1].restarttime == 10);
  CHECK(rows[1].msg1 == "own msg");
  CHECK(rows[1].msg2 == "Mod Update Available! Restart in 1 Minute.");

  // The UPDATE path is untouched: an existing row takes the values verbatim
  // (clearing msgs / backup / restarttime must NOT re-seed the defaults).
  Server upd = rows[0];
  upd.msg1 = "";
  upd.msg2 = "";
  upd.msg3 = "";
  upd.backup = 0;
  upd.restarttime = 0;
  REQUIRE(db.upsertServer(upd) == id);
  rows = db.servers();
  CHECK(rows[0].msg1 == "");
  CHECK(rows[0].msg2 == "");
  CHECK(rows[0].msg3 == "");
  CHECK(rows[0].backup == 0);
  CHECK(rows[0].restarttime == 0);
}

TEST_CASE("saveGlobalSteamcmd: partial update keeps non-given credentials") {
  Db db;
  REQUIRE(db.open(":memory:"));

  // Full write first. Values land AS GIVEN (encryption is the caller's job).
  REQUIRE(db.saveGlobalSteamcmd(0, "DPAPI:0xAA", "DPAPI:0xBB", "DPAPI:0xCC"));
  Settings g = db.settings(-1);
  REQUIRE(g.present);
  CHECK(g.steamcmdAnonymous == 0);
  CHECK(g.steamcmdUser == "DPAPI:0xAA");
  CHECK(g.steamcmdPass == "DPAPI:0xBB");
  CHECK(g.steamcmdGuard == "DPAPI:0xCC");

  // Empty pass/guard skip those columns; anonymous + user are written.
  REQUIRE(db.saveGlobalSteamcmd(1, "DPAPI:0xDD", "", ""));
  g = db.settings(-1);
  CHECK(g.steamcmdAnonymous == 1);
  CHECK(g.steamcmdUser == "DPAPI:0xDD");
  CHECK(g.steamcmdPass == "DPAPI:0xBB");   // kept
  CHECK(g.steamcmdGuard == "DPAPI:0xCC");  // kept

  // All-empty credentials: only the anonymous flag changes.
  REQUIRE(db.saveGlobalSteamcmd(0, "", "", ""));
  g = db.settings(-1);
  CHECK(g.steamcmdAnonymous == 0);
  CHECK(g.steamcmdUser == "DPAPI:0xDD");
  CHECK(g.steamcmdPass == "DPAPI:0xBB");
  CHECK(g.steamcmdGuard == "DPAPI:0xCC");

  // The per-server settings rows are untouched by the global save.
  Server sv;
  sv.name = "S";
  sv.path = "C:\\ARK\\S";
  const int64_t id = db.upsertServer(sv);
  REQUIRE(id > 0);
  REQUIRE(db.saveGlobalSteamcmd(1, "DPAPI:0xEE", "", ""));
  const Settings per = db.settings(id);
  REQUIRE(per.present);
  CHECK(per.steamcmdUser.empty());  // only server_id=-1 is written
}

TEST_CASE("launch auto_start defaults to 0 and round-trips") {
  Db db;
  REQUIRE(db.open(":memory:"));

  Server sv;
  sv.name = "S";
  sv.path = "C:\\ARK\\S";
  const int64_t id = db.upsertServer(sv);
  REQUIRE(id > 0);

  // Fresh (seeded) launch row: auto-start off, like every DDL default.
  LaunchConfig def = db.launch(id);
  CHECK(def.present);
  CHECK(def.autoStart == 0);

  // Enable and round-trip; unrelated fields keep their values.
  LaunchConfig cfg = def;
  cfg.serverId = id;
  cfg.autoStart = 1;
  cfg.desiredState = 1;
  REQUIRE(db.upsertLaunch(cfg));
  LaunchConfig r = db.launch(id);
  CHECK(r.autoStart == 1);
  CHECK(r.desiredState == 1);
  CHECK(r.game == "ASE");

  // Disable again (0 must be written back, not treated as "absent").
  cfg.autoStart = 0;
  REQUIRE(db.upsertLaunch(cfg));
  CHECK(db.launch(id).autoStart == 0);
}

TEST_CASE("stripQuotedRuns matches the AutoIt regex behavior") {
  // Matched pairs removed; an unterminated quote is kept verbatim.
  CHECK(stripQuotedRuns("a 'b' c") == "a  c");
  CHECK(stripQuotedRuns("x \"y\" z") == "x  z");
  CHECK(stripQuotedRuns("no quotes here") == "no quotes here");
  CHECK(stripQuotedRuns("'lead' and 'tail'") == " and ");
  // An unterminated quote has no closing \1, so the AutoIt regex does not match
  // it and the text (including the lone quote) is kept verbatim.
  CHECK(stripQuotedRuns("unterminated ' quote") == "unterminated ' quote");
  // Mixed: the first ' opens, next ' closes; the " inside is swallowed.
  CHECK(stripQuotedRuns("a 'has \" inside' b") == "a  b");
}
