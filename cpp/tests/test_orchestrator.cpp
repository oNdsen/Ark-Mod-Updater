#include <doctest/doctest.h>

#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "amucore/orchestrator.h"
#include "amucore/zunpack.h"

using namespace amucore;

// --- buildSteamcmdScript ------------------------------------------------------

TEST_CASE("steamcmd script: anonymous login, one mod") {
  const std::string s = buildSteamcmdScript("login anonymous", {"731604991"});
  CHECK(s ==
        "@ShutdownOnFailedCommand 1\r\n"
        "@NoPromptForPassword 1\r\n"
        "login anonymous\r\n"
        "workshop_download_item 346110 731604991 validate\r\n"
        "quit\r\n");
}

TEST_CASE("steamcmd script: credentials line is emitted verbatim") {
  const std::string s = buildSteamcmdScript("login myuser mypass ABC12", {"1"});
  CHECK(s.find("login myuser mypass ABC12\r\n") != std::string::npos);
  CHECK(s.find("login anonymous") == std::string::npos);
}

TEST_CASE("steamcmd script: many ids, order preserved, quit last") {
  const std::vector<std::string> ids = {"111", "222", "333", "444"};
  const std::string s = buildSteamcmdScript("login anonymous", ids);

  // One download line per id, in the input order.
  size_t pos = 0;
  for (const std::string& id : ids) {
    const std::string want = "workshop_download_item 346110 " + id + " validate\r\n";
    const size_t at = s.find(want);
    REQUIRE(at != std::string::npos);
    CHECK(at >= pos);
    pos = at;
  }

  // "quit" is the last line.
  CHECK(s.rfind("quit\r\n") == s.size() - 6);
}

TEST_CASE("steamcmd script: every line ends with CRLF (no bare LF)") {
  const std::string s = buildSteamcmdScript("login anonymous", {"12", "34"});
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\n') {
      REQUIRE(i > 0);
      CHECK(s[i - 1] == '\r');
    }
  }
  CHECK(s.back() == '\n');  // trailing newline like FileWriteLine
}

TEST_CASE("steamcmd script: no mod ids still forms a valid login+quit script") {
  const std::string s = buildSteamcmdScript("login anonymous", {});
  CHECK(s ==
        "@ShutdownOnFailedCommand 1\r\n"
        "@NoPromptForPassword 1\r\n"
        "login anonymous\r\n"
        "quit\r\n");
}

// --- needsReinstall truth table -----------------------------------------------
// Mirrors the two-stage _Go4Update check: (dirSize != usize OR ts differ) AND
// (dirSize != unpackedSourceSize OR ts differ).

TEST_CASE("needsReinstall: everything matches -> no reinstall") {
  CHECK_FALSE(needsReinstall(1000, 1000, "1700000000", "1700000000", 1000));
  // Unpacked size is not even consulted when the first gate says up to date.
  CHECK_FALSE(needsReinstall(1000, 1000, "1700000000", "1700000000", 9999));
}

TEST_CASE("needsReinstall: cached usize stale but source matches on disk -> no reinstall") {
  // dir size differs from the cached usize (first gate opens), but the freshly
  // computed unpacked size equals the installed size and timestamps match ->
  // the second check says the install is actually current.
  CHECK_FALSE(needsReinstall(1000, 500, "1700000000", "1700000000", 1000));
}

TEST_CASE("needsReinstall: timestamps differ -> reinstall regardless of sizes") {
  CHECK(needsReinstall(1000, 1000, "1700000000", "1700009999", 1000));
  CHECK(needsReinstall(1000, 500, "1700000000", "1700009999", 1000));
  CHECK(needsReinstall(1000, 500, "0", "1700000000", 500));
}

TEST_CASE("needsReinstall: sizes differ end to end -> reinstall") {
  CHECK(needsReinstall(1000, 500, "1700000000", "1700000000", 2000));
  CHECK(needsReinstall(0, 500, "0", "0", 2000));  // e.g. emptied install dir
}

TEST_CASE("needsReinstall: a wiped install stamped as up to date is never repaired") {
  // Why a FAILED install must NOT stamp the mods row: with the fresh usize (0,
  // read back from the broken folder) and the fresh ACF timestamp in the row,
  // the cheap first gate says "up to date" and the real source size is never
  // even consulted - the mod stays broken forever. installModFiles therefore
  // reports failure and updateServer skips the stamp.
  CHECK_FALSE(needsReinstall(0, 0, "1700000000", "1700000000", 5000));
  // With the stale row kept instead, the same folder is correctly reinstalled.
  CHECK(needsReinstall(0, 4000, "1699000000", "1700000000", 5000));
}

// --- steamcmdTimedOut (the stdout-pump watchdog) --------------------------------

TEST_CASE("steamcmdTimedOut: fires only once the idle deadline is reached") {
  CHECK_FALSE(steamcmdTimedOut(1000, 1000, 5000));  // no time passed at all
  CHECK_FALSE(steamcmdTimedOut(1000, 5999, 5000));  // 4999 ms of silence
  CHECK(steamcmdTimedOut(1000, 6000, 5000));        // exactly the deadline
  CHECK(steamcmdTimedOut(1000, 600000, 5000));
}

TEST_CASE("steamcmdTimedOut: a non-advancing clock never trips it (no underflow)") {
  // GetTickCount64 is monotonic, but subtracting the wrong way round on unsigned
  // values wraps to ~1.8e19 ms and would kill a perfectly healthy download.
  CHECK_FALSE(steamcmdTimedOut(10000, 9999, 5000));
  CHECK_FALSE(steamcmdTimedOut(10000, 0, 5000));
}

TEST_CASE("steamcmdTimedOut: the default deadline is generous but finite") {
  // A big mod downloads in complete silence, so the default must tolerate a long
  // quiet stretch - and still terminate a wedged child eventually.
  const uint64_t deadline = kSteamCmdIdleTimeoutMs;
  CHECK(deadline >= 10ull * 60 * 1000);
  CHECK_FALSE(steamcmdTimedOut(0, deadline - 1));
  CHECK(steamcmdTimedOut(0, deadline));
}

// --- unpackFailureReason --------------------------------------------------------

TEST_CASE("unpackFailureReason: names the unpackZ error code when there was one") {
  CHECK(unpackFailureReason(2) == "unpack error 2");  // bad signature/version
  CHECK(unpackFailureReason(5) == "unpack error 5");  // chunk size mismatch
}

TEST_CASE("unpackFailureReason: zerr 0 means the WRITE failed, not 'error 0'") {
  // The retry loop only writes when unpackZ succeeded, so a zero code can never
  // mean a decompression error - the first port reported "error 0" here, which
  // read like a success code.
  const std::string r = unpackFailureReason(0);
  CHECK(r == "could not write the unpacked file");
  CHECK(r.find('0') == std::string::npos);
}

// --- zHeaderUnpackedSize --------------------------------------------------------

namespace {

// Little-endian uint64 into a byte buffer at `off`.
void putU64LE(uint8_t* p, size_t off, uint64_t v) {
  for (int i = 0; i < 8; ++i) p[off + i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
}

std::vector<uint8_t> readAll(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("zHeaderUnpackedSize: synthetic 32-byte header") {
  uint8_t hdr[32] = {};
  putU64LE(hdr, 0, 2653586369ull);  // signature + format version
  putU64LE(hdr, 8, 262144);        // unpacked chunk size
  putU64LE(hdr, 16, 123);          // packed full size
  putU64LE(hdr, 24, 858);          // unpacked full size
  CHECK(zHeaderUnpackedSize(hdr, sizeof(hdr)) == 858);
}

TEST_CASE("zHeaderUnpackedSize: bad signature / short buffer -> -1") {
  uint8_t hdr[32] = {};
  putU64LE(hdr, 0, 42);  // wrong signature
  putU64LE(hdr, 24, 858);
  CHECK(zHeaderUnpackedSize(hdr, sizeof(hdr)) == -1);

  uint8_t good[32] = {};
  putU64LE(good, 0, 2653586369ull);
  CHECK(zHeaderUnpackedSize(good, 31) == -1);  // one byte short
  CHECK(zHeaderUnpackedSize(nullptr, 32) == -1);
}

TEST_CASE("zHeaderUnpackedSize: matches the golden fixture's unpacked size") {
  const auto z = readAll(std::string(AMU_FIXTURES_DIR) + "/sample.z");
  REQUIRE(z.size() >= 32);
  // sample.unpacked is 858 bytes (see test_zunpack.cpp).
  CHECK(zHeaderUnpackedSize(z.data(), z.size()) == 858);
}
