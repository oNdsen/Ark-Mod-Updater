#pragma once

#include <string>
#include <vector>

namespace amucore {

// Which ARK edition a server is. Selects the server exe and the arg dialect.
enum class ArkGame { ASE, ASA };

// Everything the command-builder needs to assemble a launch. This is a PURE
// input struct: the caller (main_sciter / orchestrator) gathers these from the
// DB launch config and from GameUserSettings.ini (the single source of truth
// for ports/RCON/session/mods) BEFORE calling buildLaunch, so buildLaunch stays
// free of I/O and fully unit-testable.
struct LaunchInput {
  ArkGame game = ArkGame::ASE;
  std::string installPath;   // ARK install ROOT (its child is "ShooterGame\...")
  std::string map;           // "TheIsland" (ASE) / "TheIsland_WP" (ASA)

  // Read live from GameUserSettings.ini; "" (or 0 for maxPlayers) means "omit".
  std::string sessionName;   // ?SessionName=
  std::string port;          // ?Port=
  std::string queryPort;     // ?QueryPort=  (ASE only; ASA uses EOS, no query port)
  bool rconEnabled = false;  // emit ?RCONEnabled=True (+ ?RCONPort=) when true
  std::string rconPort;      // ?RCONPort=
  std::string multiHome;     // bind IP; "" or "0" == none
  std::string activeMods;    // comma id list (ASA -> -mods=; ASE loads via ini, not CLI)
  int maxPlayers = 0;        // ASE -> ?MaxPlayers= ; ASA -> -WinLiveMaxPlayers= ; 0 = omit

  // From the DB launch table (toggles / advanced).
  bool battleye = true;      // false -> -NoBattlEye
  bool crossplay = false;    // ASA -> -ServerPlatform=ALL when true, else PC
  bool autoManaged = false;  // ASE -> -automanagedmods
  std::string clusterId;     // -clusterid=
  std::string clusterDir;    // -ClusterDirOverride=
  std::string perfFlags;     // verbatim, space-separated (e.g. "-USEALLAVAILABLECORES")
  std::string extraFlags;    // verbatim, space-separated
  std::string extraArgs;     // verbatim tail, space-separated
};

// The assembled command. `args` is the argv AFTER the exe (handy for tests);
// `commandLine` is the full CreateProcessW lpCommandLine (quoted exe + args).
struct LaunchCommand {
  std::string exePath;      // <install>\ShooterGame\Binaries\Win64\<exe>
  std::string workingDir;   // <install>\ShooterGame\Binaries\Win64
  std::vector<std::string> args;
  std::string commandLine;
};

// Assemble the launch for `in`. Deterministic option order:
//   URL: <map>?listen[?SessionName][?Port][?QueryPort(ASE)][?RCONEnabled=True]
//        [?RCONPort][?MaxPlayers(ASE)][?MultiHome(ASA)]
//   dash flags: -server -log [-MULTIHOME(ASE)] [-NoBattlEye] [-automanagedmods(ASE)]
//        [-WinLiveMaxPlayers(ASA)] [-ServerPlatform(ASA)] [-mods(ASA)]
//        [-clusterid] [-ClusterDirOverride] <perfFlags> <extraFlags> <extraArgs>
LaunchCommand buildLaunch(const LaunchInput& in);

// Quote one argv token per the Windows CommandLineToArgvW rules (backslash/quote
// aware). Tokens with no whitespace or quotes are returned unchanged.
std::string quoteArg(const std::string& a);

// Server exe file name for a game ("ShooterGameServer.exe" / "ArkAscendedServer.exe").
std::string serverExeName(ArkGame game);

}  // namespace amucore
