#include <doctest/doctest.h>

#include "amucore/steamcmd_parser.h"

using namespace amucore;

TEST_CASE("success line in one chunk") {
  SteamCmdParser p;
  auto ev = p.feed("Success. Downloaded item 731604991 to \"x\" (245 bytes) after 1s\n");
  REQUIRE(ev.size() == 1);
  CHECK(ev[0].type == SteamCmdEvent::Type::DownloadSucceeded);
  CHECK(ev[0].modId == "731604991");
  CHECK(ev[0].bytes == 245);
}

TEST_CASE("success split across two chunks") {
  SteamCmdParser p;
  CHECK(p.feed("Success. Downloaded it").empty());
  auto ev = p.feed("em 895711211 (1024 bytes)\n");
  REQUIRE(ev.size() == 1);
  CHECK(ev[0].modId == "895711211");
  CHECK(ev[0].bytes == 1024);
}

TEST_CASE("two events reported in one chunk") {
  SteamCmdParser p;
  auto ev = p.feed("Downloading item 111\nSuccess. Downloaded item 111 (5 bytes)\n");
  REQUIRE(ev.size() == 2);
  CHECK(ev[0].type == SteamCmdEvent::Type::Downloading);
  CHECK(ev[0].modId == "111");
  CHECK(ev[1].type == SteamCmdEvent::Type::DownloadSucceeded);
}

TEST_CASE("failed with File Not Found is flagged removed") {
  SteamCmdParser p;
  auto ev = p.feed("ERROR! Download item 632091170 failed (File Not Found).\n");
  REQUIRE(ev.size() == 1);
  CHECK(ev[0].type == SteamCmdEvent::Type::DownloadFailed);
  CHECK(ev[0].modId == "632091170");
  CHECK(ev[0].removed);
}

TEST_CASE("failed with another reason is not removed") {
  SteamCmdParser p;
  auto ev = p.feed("ERROR! Download item 42 failed (Timeout).\n");
  REQUIRE(ev.size() == 1);
  CHECK_FALSE(ev[0].removed);
  CHECK(ev[0].reason == "Timeout");
}

TEST_CASE("trailing line without newline needs flush()") {
  SteamCmdParser p;
  CHECK(p.feed("Success. Downloaded item 7 (9 bytes)").empty());
  auto ev = p.flush();
  REQUIRE(ev.size() == 1);
  CHECK(ev[0].modId == "7");
}

TEST_CASE("CRLF line endings are handled") {
  SteamCmdParser p;
  auto ev = p.feed("Success. Downloaded item 8 (3 bytes)\r\n");
  REQUIRE(ev.size() == 1);
  CHECK(ev[0].modId == "8");
}
