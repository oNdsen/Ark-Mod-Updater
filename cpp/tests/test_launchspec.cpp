#include <doctest/doctest.h>

#include <algorithm>
#include <string>

#include "amucore/launchspec.h"

using namespace amucore;

static bool has(const std::vector<std::string>& v, const std::string& t) {
  return std::find(v.begin(), v.end(), t) != v.end();
}
static bool hasPrefix(const std::vector<std::string>& v, const std::string& p) {
  for (const auto& s : v)
    if (s.rfind(p, 0) == 0) return true;
  return false;
}

TEST_CASE("buildLaunch ASE full command line") {
  LaunchInput in;
  in.game = ArkGame::ASE;
  in.installPath = "D:\\ARK\\srv1";
  in.map = "TheIsland";
  in.sessionName = "My ARK Server";
  in.port = "7777";
  in.queryPort = "27015";
  in.rconEnabled = true;
  in.rconPort = "32330";
  in.maxPlayers = 20;
  in.battleye = false;

  const auto c = buildLaunch(in);
  CHECK(c.exePath == "D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe");
  CHECK(c.workingDir == "D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64");
  CHECK(c.args[0] ==
        "TheIsland?listen?SessionName=My ARK Server?Port=7777?QueryPort=27015"
        "?RCONEnabled=True?RCONPort=32330?MaxPlayers=20");
  CHECK(has(c.args, "-server"));
  CHECK(has(c.args, "-log"));
  CHECK(has(c.args, "-NoBattlEye"));
  // The whole URL token has a space (SessionName) -> quoted as one unit.
  CHECK(c.commandLine ==
        "\"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe\" "
        "\"TheIsland?listen?SessionName=My ARK Server?Port=7777?QueryPort=27015"
        "?RCONEnabled=True?RCONPort=32330?MaxPlayers=20\" -server -log -NoBattlEye");
}

TEST_CASE("buildLaunch ASE minimal (no options)") {
  LaunchInput in;
  in.game = ArkGame::ASE;
  in.installPath = "C:\\srv";
  in.map = "TheIsland";

  const auto c = buildLaunch(in);
  CHECK(c.args.size() == 3);
  CHECK(c.args[0] == "TheIsland?listen");
  CHECK(c.args[1] == "-server");
  CHECK(c.args[2] == "-log");
  // No spaces in the URL -> not quoted.
  CHECK(c.commandLine ==
        "\"C:\\srv\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe\" "
        "TheIsland?listen -server -log");
}

TEST_CASE("buildLaunch ASE does NOT put mods on the CLI (ActiveMods is ini-only)") {
  LaunchInput in;
  in.game = ArkGame::ASE;
  in.installPath = "C:\\srv";
  in.map = "TheIsland";
  in.activeMods = "731604991,895711211";
  in.autoManaged = true;

  const auto c = buildLaunch(in);
  CHECK_FALSE(hasPrefix(c.args, "-mods="));  // ASE loads mods via GameUserSettings.ini
  CHECK(has(c.args, "-automanagedmods"));
}

TEST_CASE("buildLaunch ASE MultiHome uses the -MULTIHOME dash flag, not a query opt") {
  LaunchInput in;
  in.game = ArkGame::ASE;
  in.installPath = "C:\\srv";
  in.map = "TheIsland";
  in.multiHome = "192.168.1.50";

  const auto c = buildLaunch(in);
  CHECK(has(c.args, "-MULTIHOME"));
  CHECK(c.args[0].find("MultiHome") == std::string::npos);  // not in the URL for ASE

  in.multiHome = "0";  // "0" == none -> no flag
  CHECK_FALSE(has(buildLaunch(in).args, "-MULTIHOME"));
}

TEST_CASE("buildLaunch ASA full: exe, _WP map, no QueryPort, -WinLiveMaxPlayers, -mods, platform") {
  LaunchInput in;
  in.game = ArkGame::ASA;
  in.installPath = "E:\\ASA";
  in.map = "TheIsland_WP";
  in.sessionName = "Rag";
  in.port = "7779";
  in.queryPort = "27017";  // must be ignored for ASA
  in.rconEnabled = true;
  in.rconPort = "27020";
  in.maxPlayers = 50;
  in.battleye = false;
  in.crossplay = true;
  in.activeMods = "935528,990263";
  in.multiHome = "10.0.0.2";

  const auto c = buildLaunch(in);
  CHECK(c.exePath == "E:\\ASA\\ShooterGame\\Binaries\\Win64\\ArkAscendedServer.exe");
  CHECK(c.args[0] ==
        "TheIsland_WP?listen?SessionName=Rag?Port=7779?RCONEnabled=True?RCONPort=27020"
        "?MultiHome=10.0.0.2");
  CHECK(c.args[0].find("QueryPort") == std::string::npos);   // EOS, no query port
  CHECK(c.args[0].find("MaxPlayers") == std::string::npos);  // ASA uses the dash flag
  CHECK(has(c.args, "-WinLiveMaxPlayers=50"));
  CHECK(has(c.args, "-ServerPlatform=ALL"));
  CHECK(has(c.args, "-mods=935528,990263"));
  CHECK(has(c.args, "-NoBattlEye"));
  CHECK_FALSE(has(c.args, "-MULTIHOME"));       // ASA uses ?MultiHome, not the dash flag
  CHECK_FALSE(has(c.args, "-automanagedmods"));  // ASE-only
}

TEST_CASE("buildLaunch ASA platform PC when crossplay off") {
  LaunchInput in;
  in.game = ArkGame::ASA;
  in.installPath = "E:\\ASA";
  in.map = "TheCenter_WP";
  in.crossplay = false;

  CHECK(has(buildLaunch(in).args, "-ServerPlatform=PC"));
}

TEST_CASE("buildLaunch cluster + verbatim perf/extra flags, in order") {
  LaunchInput in;
  in.game = ArkGame::ASE;
  in.installPath = "C:\\srv";
  in.map = "Ragnarok";
  in.clusterId = "myc";
  in.clusterDir = "D:\\ArkCluster";
  in.perfFlags = "-USEALLAVAILABLECORES  -nomemorybias";  // double space -> still two tokens
  in.extraArgs = "-foo -bar";

  const auto c = buildLaunch(in);
  CHECK(has(c.args, "-clusterid=myc"));
  CHECK(has(c.args, "-ClusterDirOverride=D:\\ArkCluster"));
  CHECK(has(c.args, "-USEALLAVAILABLECORES"));
  CHECK(has(c.args, "-nomemorybias"));
  CHECK(has(c.args, "-foo"));
  CHECK(has(c.args, "-bar"));
}

TEST_CASE("quoteArg Windows rules") {
  CHECK(quoteArg("abc") == "abc");                 // no whitespace -> unchanged
  CHECK(quoteArg("a\\b\\c") == "a\\b\\c");          // backslashes without spaces -> unchanged
  CHECK(quoteArg("a b") == "\"a b\"");              // space -> wrapped
  CHECK(quoteArg("a\"b") == "\"a\\\"b\"");          // embedded quote escaped
  CHECK(quoteArg("a b\\") == "\"a b\\\\\"");        // trailing backslash doubled before close
  CHECK(quoteArg("") == "\"\"");                    // empty -> explicit empty token
}
