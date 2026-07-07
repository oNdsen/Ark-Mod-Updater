#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "amucore/serverconfig.h"

using namespace amucore;

namespace {

std::string readWholeFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Copy the fixture ini to a fresh temp path so writes don't touch the fixture
// (and never dirty the source tree - the temp file lands in the OS temp dir).
std::string makeTempCopy(const std::string& src, const std::string& tag) {
  const std::string dst =
      (std::filesystem::temp_directory_path() / ("_amu_serverconfig_" + tag + ".ini")).string();
  std::ofstream out(dst, std::ios::binary | std::ios::trunc);
  out << readWholeFile(src);
  out.close();
  return dst;
}

}  // namespace

TEST_CASE("parseActiveMods splits, trims and drops empties") {
  auto ids = parseActiveMods(" 731604991 , 895711211,, 924933745 ");
  REQUIRE(ids.size() == 3);
  CHECK(ids[0] == "731604991");
  CHECK(ids[1] == "895711211");
  CHECK(ids[2] == "924933745");

  CHECK(parseActiveMods("").empty());
  CHECK(parseActiveMods("   ").empty());
  CHECK(parseActiveMods(",,,").empty());

  auto one = parseActiveMods("111111111");
  REQUIRE(one.size() == 1);
  CHECK(one[0] == "111111111");
}

TEST_CASE("joinActiveMods produces the on-disk comma form without spaces") {
  CHECK(joinActiveMods({"731604991", "895711211", "924933745"}) ==
        "731604991,895711211,924933745");
  CHECK(joinActiveMods({}) == "");
  // empty / whitespace entries are skipped, no stray commas
  CHECK(joinActiveMods({"111", "", "  ", "222"}) == "111,222");
}

TEST_CASE("addModId dedups and appends") {
  std::vector<std::string> ids = {"111", "222"};
  bool added = false;

  auto same = addModId(ids, "222", &added);
  CHECK(added == false);
  CHECK(same == ids);

  auto grown = addModId(ids, "333", &added);
  CHECK(added == true);
  REQUIRE(grown.size() == 3);
  CHECK(grown[2] == "333");

  // dedup compares trimmed values
  auto trimmedDup = addModId(ids, " 111 ", &added);
  CHECK(added == false);
  CHECK(trimmedDup == ids);
}

TEST_CASE("removeModId drops matching ids and empties") {
  std::vector<std::string> ids = {"111", "222", "333"};
  bool removed = false;

  auto out = removeModId(ids, "222", &removed);
  CHECK(removed == true);
  REQUIRE(out.size() == 2);
  CHECK(out[0] == "111");
  CHECK(out[1] == "333");

  auto absent = removeModId(ids, "999", &removed);
  CHECK(removed == false);
  CHECK(absent.size() == 3);

  // trimmed match + empties dropped
  std::vector<std::string> messy = {" 111 ", "", "222"};
  auto cleaned = removeModId(messy, "111", &removed);
  CHECK(removed == true);
  REQUIRE(cleaned.size() == 1);
  CHECK(cleaned[0] == "222");
}

TEST_CASE("readActiveMods parses the fixture GameUserSettings.ini") {
  const std::string ini = std::string(AMU_FIXTURES_DIR) + "/GameUserSettings.ini";
  auto ids = readActiveMods(ini);
  REQUIRE(ids.size() == 3);
  CHECK(ids[0] == "731604991");
  CHECK(ids[1] == "895711211");
  CHECK(ids[2] == "924933745");
}

TEST_CASE("writeActiveMods round-trips and preserves other keys") {
  const std::string ini = makeTempCopy(std::string(AMU_FIXTURES_DIR) + "/GameUserSettings.ini",
                                       "rt");

  std::vector<std::string> newIds = {"111111111", "222222222"};
  REQUIRE(writeActiveMods(ini, newIds));

  auto back = readActiveMods(ini);
  REQUIRE(back.size() == 2);
  CHECK(back[0] == "111111111");
  CHECK(back[1] == "222222222");

  // An unrelated key in the same and other section survives the write.
  const std::string contents = readWholeFile(ini);
  CHECK(contents.find("RCONEnabled=True") != std::string::npos);
  CHECK(contents.find("SessionName=My Ultimate ARK Server") != std::string::npos);
}

TEST_CASE("addActiveMod / removeActiveMod convenience helpers on a temp file") {
  const std::string ini = makeTempCopy(std::string(AMU_FIXTURES_DIR) + "/GameUserSettings.ini",
                                       "helpers");

  // adding an already-active id is a no-op (returns false, no change)
  CHECK(addActiveMod(ini, "731604991") == false);
  CHECK(readActiveMods(ini).size() == 3);

  // adding a new id appends it
  CHECK(addActiveMod(ini, "555555555") == true);
  auto afterAdd = readActiveMods(ini);
  REQUIRE(afterAdd.size() == 4);
  CHECK(afterAdd[3] == "555555555");

  // removing an existing id drops it
  CHECK(removeActiveMod(ini, "895711211") == true);
  auto afterRemove = readActiveMods(ini);
  REQUIRE(afterRemove.size() == 3);
  CHECK(afterRemove[0] == "731604991");
  CHECK(afterRemove[1] == "924933745");
  CHECK(afterRemove[2] == "555555555");

  // removing an absent id returns false but still normalizes/writes
  CHECK(removeActiveMod(ini, "999999999") == false);
  CHECK(readActiveMods(ini).size() == 3);
}

namespace {

// RAII temp ini in the system temp dir; deleted when the test case ends.
struct TempIni {
  std::string path;
  explicit TempIni(const char* name, const std::string& contents) {
    path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
  }
  ~TempIni() { std::filesystem::remove(std::filesystem::path(path)); }
};

}  // namespace

TEST_CASE("iniReadAll preserves section order, entry order and duplicate keys") {
  // Duplicate keys are the ARK array idiom (e.g. repeated OverrideEngramEntries
  // lines in Game.ini); values may themselves contain '='.
  TempIni tmp("_amu_test_inireadall.ini",
              "[ServerSettings]\r\n"
              "RCONEnabled=True\r\n"
              "Formula=a=b=c\r\n"
              "EmptyVal=\r\n"
              "[/script/shootergame.shootergamemode]\r\n"
              "OverrideEngramEntries=(EngramIndex=0,EngramHidden=true)\r\n"
              "OverrideEngramEntries=(EngramIndex=1,EngramHidden=false)\r\n"
              "MatingIntervalMultiplier=0.5\r\n");

  const auto sections = iniReadAll(tmp.path);
  REQUIRE(sections.size() == 2);

  CHECK(sections[0].name == "ServerSettings");
  REQUIRE(sections[0].entries.size() == 3);
  CHECK(sections[0].entries[0].key == "RCONEnabled");
  CHECK(sections[0].entries[0].value == "True");
  // split happens on the FIRST '=' only
  CHECK(sections[0].entries[1].key == "Formula");
  CHECK(sections[0].entries[1].value == "a=b=c");
  CHECK(sections[0].entries[2].key == "EmptyVal");
  CHECK(sections[0].entries[2].value == "");

  CHECK(sections[1].name == "/script/shootergame.shootergamemode");
  REQUIRE(sections[1].entries.size() == 3);
  // both duplicate lines survive, in file order
  CHECK(sections[1].entries[0].key == "OverrideEngramEntries");
  CHECK(sections[1].entries[0].value == "(EngramIndex=0,EngramHidden=true)");
  CHECK(sections[1].entries[1].key == "OverrideEngramEntries");
  CHECK(sections[1].entries[1].value == "(EngramIndex=1,EngramHidden=false)");
  CHECK(sections[1].entries[2].key == "MatingIntervalMultiplier");
  CHECK(sections[1].entries[2].value == "0.5");
}

TEST_CASE("iniReadAll on a missing file returns no sections") {
  const std::string missing =
      (std::filesystem::temp_directory_path() / "_amu_test_does_not_exist.ini").string();
  CHECK(iniReadAll(missing).empty());
}

TEST_CASE("iniDeleteKey removes exactly one key and keeps the rest") {
  TempIni tmp("_amu_test_inidelete.ini",
              "[ServerSettings]\r\n"
              "DifficultyOffset=1.0\r\n"
              "TamingSpeedMultiplier=3.0\r\n"
              "XPMultiplier=2.0\r\n"
              "[SessionSettings]\r\n"
              "Port=7777\r\n");

  CHECK(iniDeleteKey(tmp.path, "ServerSettings", "TamingSpeedMultiplier"));

  const auto sections = iniReadAll(tmp.path);
  REQUIRE(sections.size() == 2);
  REQUIRE(sections[0].entries.size() == 2);
  CHECK(sections[0].entries[0].key == "DifficultyOffset");
  CHECK(sections[0].entries[1].key == "XPMultiplier");
  CHECK(sections[1].entries.size() == 1);
  CHECK(sections[1].entries[0].key == "Port");

  // deleting an already-absent key still succeeds and changes nothing
  CHECK(iniDeleteKey(tmp.path, "ServerSettings", "TamingSpeedMultiplier"));
  CHECK(iniReadAll(tmp.path)[0].entries.size() == 2);
}

// --- replaceKeyLinesInText (raw ARK array-family rewrite, pure core) ----------

TEST_CASE("replaceKeyLinesInText replaces a repeated-key family at the first position") {
  const std::string in =
      "[ServerSettings]\r\n"
      "RCONEnabled=True\r\n"
      "[/script/shootergame.shootergamemode]\r\n"
      "bAllowFlyerCarryPvE=True\r\n"
      "OverrideNamedEngramEntries=(EngramClassName=\"A\")\r\n"
      "OverrideNamedEngramEntries=(EngramClassName=\"B\")\r\n"
      "MatingIntervalMultiplier=0.5\r\n"
      "OverrideNamedEngramEntries=(EngramClassName=\"C\")\r\n";
  const auto out = replaceKeyLinesInText(
      in, "/script/shootergame.shootergamemode", "OverrideNamedEngramEntries",
      {"OverrideNamedEngramEntries=(EngramClassName=\"X\")",
       "OverrideNamedEngramEntries=(EngramClassName=\"Y\")"});
  // ALL family lines removed (even the one after an unrelated key); the new
  // lines sit exactly where the first removed line was; the rest is untouched.
  CHECK(out ==
        "[ServerSettings]\r\n"
        "RCONEnabled=True\r\n"
        "[/script/shootergame.shootergamemode]\r\n"
        "bAllowFlyerCarryPvE=True\r\n"
        "OverrideNamedEngramEntries=(EngramClassName=\"X\")\r\n"
        "OverrideNamedEngramEntries=(EngramClassName=\"Y\")\r\n"
        "MatingIntervalMultiplier=0.5\r\n");
}

TEST_CASE("replaceKeyLinesInText replaces an [i]-indexed family, not lookalike keys") {
  const std::string in =
      "[/script/shootergame.shootergamemode]\r\n"
      "PerLevelStatsMultiplier_Player[0]=1.0\r\n"
      "PerLevelStatsMultiplier_Player[1]=1.5\r\n"
      "PerLevelStatsMultiplier_PlayerBase=9.9\r\n"  // key + other char: NOT family
      "PerLevelStatsMultiplier_Player=0.1\r\n"       // exact key: IS family
      "XPMultiplier=2.0\r\n";
  const auto out = replaceKeyLinesInText(
      in, "/script/shootergame.shootergamemode", "PerLevelStatsMultiplier_Player",
      {"PerLevelStatsMultiplier_Player[0]=2.0", "PerLevelStatsMultiplier_Player[7]=3.0"});
  CHECK(out ==
        "[/script/shootergame.shootergamemode]\r\n"
        "PerLevelStatsMultiplier_Player[0]=2.0\r\n"
        "PerLevelStatsMultiplier_Player[7]=3.0\r\n"
        "PerLevelStatsMultiplier_PlayerBase=9.9\r\n"
        "XPMultiplier=2.0\r\n");
}

TEST_CASE("replaceKeyLinesInText: no match appends at the section end, comments survive") {
  const std::string in =
      "[A]\r\n"
      "; a comment line\r\n"
      "x=1\r\n"
      "\r\n"
      "[B]\r\n"
      "y=2\r\n";
  const auto out = replaceKeyLinesInText(in, "A", "NewFamily", {"NewFamily[0]=7"});
  // Inserted before the next section header; comment + blank line untouched.
  CHECK(out ==
        "[A]\r\n"
        "; a comment line\r\n"
        "x=1\r\n"
        "\r\n"
        "NewFamily[0]=7\r\n"
        "[B]\r\n"
        "y=2\r\n");
}

TEST_CASE("replaceKeyLinesInText creates a missing section at EOF") {
  // Also: the existing last line has no trailing newline -> it gets one first.
  const std::string in = "[A]\r\nx=1";
  const auto out = replaceKeyLinesInText(in, "NewSec", "K", {"K[0]=1", "K[1]=2"});
  CHECK(out == "[A]\r\nx=1\r\n[NewSec]\r\nK[0]=1\r\nK[1]=2\r\n");

  // Empty input text: header + lines from scratch (CRLF default).
  CHECK(replaceKeyLinesInText("", "Sec", "K", {"K=1"}) == "[Sec]\r\nK=1\r\n");

  // Empty lines + missing section: nothing to remove, nothing created.
  CHECK(replaceKeyLinesInText(in, "NewSec", "K", {}) == in);
}

TEST_CASE("replaceKeyLinesInText only touches the family in the target section") {
  const std::string in =
      "[A]\r\n"
      "Shared=1\r\n"
      "[B]\r\n"
      "Shared=2\r\n";
  const auto out = replaceKeyLinesInText(in, "B", "Shared", {"Shared=9"});
  CHECK(out ==
        "[A]\r\n"
        "Shared=1\r\n"
        "[B]\r\n"
        "Shared=9\r\n");
}

TEST_CASE("replaceKeyLinesInText with an empty array removes the family") {
  const std::string in =
      "[A]\r\n"
      "Fam[0]=1\r\n"
      "keep=1\r\n"
      "Fam=2\r\n"
      "[B]\r\n"
      "Fam=3\r\n";
  const auto out = replaceKeyLinesInText(in, "A", "Fam", {});
  CHECK(out ==
        "[A]\r\n"
        "keep=1\r\n"
        "[B]\r\n"
        "Fam=3\r\n");
}

TEST_CASE("replaceKeyLinesInText preserves LF files and inserts LF lines") {
  const std::string in =
      "[A]\n"
      "Fam=1\n"
      "keep=2\n";
  const auto out = replaceKeyLinesInText(in, "A", "Fam", {"Fam=9"});
  CHECK(out ==
        "[A]\n"
        "Fam=9\n"
        "keep=2\n");
}

TEST_CASE("replaceKeyLinesInText matches section and key case-insensitively") {
  const std::string in =
      "[serversettings]\r\n"
      "activemods=1,2\r\n";
  const auto out = replaceKeyLinesInText(in, "ServerSettings", "ActiveMods",
                                         {"ActiveMods=3,4"});
  CHECK(out ==
        "[serversettings]\r\n"
        "ActiveMods=3,4\r\n");
}

// --- iniReplaceKeyLines (file wrapper: encoding + atomic-ish write) -----------

namespace {

// UTF-16LE bytes with BOM for a wide string (wchar_t is 2 bytes on Windows).
std::string utf16LeBytes(const std::wstring& w) {
  std::string b;
  b.push_back('\xFF');
  b.push_back('\xFE');
  b.append(reinterpret_cast<const char*>(w.data()), w.size() * 2);
  return b;
}

}  // namespace

TEST_CASE("iniReplaceKeyLines: 8-bit file round-trips byte-for-byte outside the edit") {
  TempIni tmp("_amu_test_replacelines_8bit.ini",
              "[A]\r\n"
              "x=1\r\n"
              "Fam[0]=1\r\n"
              "[B]\r\n"
              "y=2\r\n");
  REQUIRE(iniReplaceKeyLines(tmp.path, "A", "Fam", {"Fam[0]=9", "Fam[1]=8"}));
  const std::string raw = readWholeFile(tmp.path);
  CHECK(raw ==
        "[A]\r\n"
        "x=1\r\n"
        "Fam[0]=9\r\n"
        "Fam[1]=8\r\n"
        "[B]\r\n"
        "y=2\r\n");
  // No BOM was introduced.
  REQUIRE(!raw.empty());
  CHECK(raw[0] == '[');
}

TEST_CASE("iniReplaceKeyLines: UTF-16LE BOM file stays UTF-16LE with BOM") {
  TempIni tmp("_amu_test_replacelines_u16.ini",
              utf16LeBytes(L"[ServerSettings]\r\n"
                           L"SessionName=M\x00E4p Server\r\n"  // non-ASCII survives
                           L"Fam=1\r\n"));
  REQUIRE(iniReplaceKeyLines(tmp.path, "ServerSettings", "Fam", {"Fam=2"}));

  const std::string raw = readWholeFile(tmp.path);
  REQUIRE(raw.size() >= 2);
  CHECK(static_cast<unsigned char>(raw[0]) == 0xFF);
  CHECK(static_cast<unsigned char>(raw[1]) == 0xFE);

  std::wstring wide((raw.size() - 2) / 2, L'\0');
  std::memcpy(wide.data(), raw.data() + 2, wide.size() * 2);
  CHECK(wide ==
        L"[ServerSettings]\r\n"
        L"SessionName=M\x00E4p Server\r\n"
        L"Fam=2\r\n");
}

TEST_CASE("iniReplaceKeyLines creates a missing file (8-bit, CRLF)") {
  const std::string path =
      (std::filesystem::temp_directory_path() / "_amu_test_replacelines_new.ini").string();
  std::filesystem::remove(std::filesystem::path(path));
  REQUIRE(iniReplaceKeyLines(path, "Sec", "K", {"K=1"}));
  CHECK(readWholeFile(path) == "[Sec]\r\nK=1\r\n");
  std::filesystem::remove(std::filesystem::path(path));
}
