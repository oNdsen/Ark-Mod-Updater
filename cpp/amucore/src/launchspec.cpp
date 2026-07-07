#include "amucore/launchspec.h"

namespace amucore {

namespace {

// Append "?Key=Value" to the URL token when value is non-empty.
void appendOpt(std::string& url, const char* key, const std::string& value) {
  if (value.empty()) return;
  url += '?';
  url += key;
  url += '=';
  url += value;
}

// Split a verbatim flag string on ASCII whitespace into individual argv tokens.
// (ARK flags never contain interior spaces; a value that needs a space would be
// a single token the user must quote themselves via extraArgs.)
void appendVerbatim(std::vector<std::string>& args, const std::string& s) {
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    size_t start = i;
    while (i < s.size() && s[i] != ' ' && s[i] != '\t') ++i;
    if (i > start) args.push_back(s.substr(start, i - start));
  }
}

bool multiHomeSet(const std::string& mh) { return !mh.empty() && mh != "0"; }

}  // namespace

std::string serverExeName(ArkGame game) {
  return game == ArkGame::ASA ? "ArkAscendedServer.exe" : "ShooterGameServer.exe";
}

std::string quoteArg(const std::string& a) {
  // Fast path: no quoting needed when there is no whitespace or quote.
  if (!a.empty() && a.find_first_of(" \t\n\v\"") == std::string::npos) return a;

  std::string r = "\"";
  for (size_t i = 0;; ++i) {
    size_t backslashes = 0;
    while (i < a.size() && a[i] == '\\') {
      ++backslashes;
      ++i;
    }
    if (i == a.size()) {
      // Escape all trailing backslashes so they do not escape the closing quote.
      r.append(backslashes * 2, '\\');
      break;
    } else if (a[i] == '"') {
      // Escape the backslashes AND the quote.
      r.append(backslashes * 2 + 1, '\\');
      r.push_back('"');
    } else {
      r.append(backslashes, '\\');
      r.push_back(a[i]);
    }
  }
  r.push_back('"');
  return r;
}

LaunchCommand buildLaunch(const LaunchInput& in) {
  const bool asa = in.game == ArkGame::ASA;

  LaunchCommand out;
  const std::string binDir = in.installPath + "\\ShooterGame\\Binaries\\Win64";
  out.workingDir = binDir;
  out.exePath = binDir + "\\" + serverExeName(in.game);

  // --- the map URL token -----------------------------------------------------
  std::string url = in.map + "?listen";
  appendOpt(url, "SessionName", in.sessionName);
  appendOpt(url, "Port", in.port);
  if (!asa) appendOpt(url, "QueryPort", in.queryPort);  // ASA uses EOS, no query port
  if (in.rconEnabled) {
    url += "?RCONEnabled=True";
    appendOpt(url, "RCONPort", in.rconPort);
  }
  if (!asa && in.maxPlayers > 0) appendOpt(url, "MaxPlayers", std::to_string(in.maxPlayers));
  if (asa && multiHomeSet(in.multiHome)) appendOpt(url, "MultiHome", in.multiHome);
  out.args.push_back(url);

  // --- dash flags ------------------------------------------------------------
  out.args.push_back("-server");
  out.args.push_back("-log");
  if (!asa && multiHomeSet(in.multiHome)) out.args.push_back("-MULTIHOME");
  if (!in.battleye) out.args.push_back("-NoBattlEye");
  if (!asa && in.autoManaged) out.args.push_back("-automanagedmods");
  if (asa) {
    if (in.maxPlayers > 0) out.args.push_back("-WinLiveMaxPlayers=" + std::to_string(in.maxPlayers));
    out.args.push_back(std::string("-ServerPlatform=") + (in.crossplay ? "ALL" : "PC"));
    if (!in.activeMods.empty()) out.args.push_back("-mods=" + in.activeMods);
  }
  if (!in.clusterId.empty()) out.args.push_back("-clusterid=" + in.clusterId);
  if (!in.clusterDir.empty()) out.args.push_back("-ClusterDirOverride=" + in.clusterDir);
  appendVerbatim(out.args, in.perfFlags);
  appendVerbatim(out.args, in.extraFlags);
  appendVerbatim(out.args, in.extraArgs);

  // --- flat CreateProcessW command line --------------------------------------
  // argv[0] (the exe) is ALWAYS quoted: real install paths can contain spaces
  // (e.g. "C:\Program Files\..."), and the exe path never ends in a backslash or
  // contains a quote, so a plain wrap is correct.
  out.commandLine = "\"" + out.exePath + "\"";
  for (const auto& a : out.args) {
    out.commandLine += ' ';
    out.commandLine += quoteArg(a);
  }
  return out;
}

}  // namespace amucore
