#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "amucore/updatecheck.h"

using namespace amucore;

// ---------------------------------------------------------------------------
// parseManifest
// ---------------------------------------------------------------------------

TEST_CASE("parseManifest reads valid lines and carries the version") {
  const std::string sha1(64, 'a');
  const std::string sha2(64, 'b');
  const std::string text =
      "amu.exe|" + sha1 + "|12345\n" +
      "sciter.dll|" + sha2 + "|999\n";

  const Manifest m = parseManifest(text, "0.10.0");
  CHECK(m.version == "0.10.0");
  REQUIRE(m.files.size() == 2);
  CHECK(m.files[0].path == "amu.exe");
  CHECK(m.files[0].sha256 == sha1);
  CHECK(m.files[0].size == 12345);
  CHECK(m.files[1].path == "sciter.dll");
  CHECK(m.files[1].sha256 == sha2);
  CHECK(m.files[1].size == 999);
}

TEST_CASE("parseManifest skips comments and blank lines") {
  const std::string sha(64, '0');
  const std::string text =
      "# this is a comment\n"
      "\n"
      "   \n"
      "  # indented comment\n"
      "ui/main.html|" + sha + "|42\n"
      "\r\n";

  const Manifest m = parseManifest(text, "1.0");
  REQUIRE(m.files.size() == 1);
  CHECK(m.files[0].path == "ui/main.html");
  CHECK(m.files[0].size == 42);
}

TEST_CASE("parseManifest drops malformed lines") {
  const std::string sha(64, 'c');
  const std::string text =
      "justonefield\n"                              // no separators
      "two|fields\n"                                // only two fields
      "a.exe|" + sha + "|10|extra\n"                // four fields
      "b.exe|deadbeef|10\n"                         // hash too short
      "c.exe|" + std::string(64, 'x') + "|10\n"     // not hex
      "d.exe|" + sha + "|notanumber\n"              // size not numeric
      "e.exe|" + sha + "|-5\n"                      // negative size
      "|" + sha + "|10\n"                           // empty path
      "ok.exe|" + sha + "|10\n";                    // the only good line

  const Manifest m = parseManifest(text, "1.0");
  REQUIRE(m.files.size() == 1);
  CHECK(m.files[0].path == "ok.exe");
}

TEST_CASE("parseManifest accepts both slash styles and CRLF endings") {
  const std::string sha(64, 'd');
  const std::string text =
      "ui/main.html|" + sha + "|1\r\n"
      "lib\\sciter.dll|" + sha + "|2\r\n";

  const Manifest m = parseManifest(text, "1.0");
  REQUIRE(m.files.size() == 2);
  CHECK(m.files[0].path == "ui/main.html");
  CHECK(m.files[1].path == "lib\\sciter.dll");
}

TEST_CASE("parseManifest normalizes uppercase hashes to lowercase") {
  const std::string text = "amu.exe|" + std::string(64, 'A') + "|7\n";
  const Manifest m = parseManifest(text, "1.0");
  REQUIRE(m.files.size() == 1);
  CHECK(m.files[0].sha256 == std::string(64, 'a'));
}

TEST_CASE("parseManifest of empty/comment-only text yields no files") {
  CHECK(parseManifest("", "1.0").files.empty());
  CHECK(parseManifest("# nothing\n# here\n", "1.0").files.empty());
}

TEST_CASE("parseManifest drops a size too large for int64 instead of overflowing") {
  // The manifest is downloaded; "v = v * 10 + digit" without a bound is signed
  // overflow (UB) on a long digit run.
  const std::string sha(64, 'e');
  const std::string text =
      "huge.bin|" + sha + "|" + std::string(40, '9') + "\n" +
      "ok.exe|" + sha + "|10\n";

  const Manifest m = parseManifest(text, "1.0");
  REQUIRE(m.files.size() == 1);
  CHECK(m.files[0].path == "ok.exe");
}

TEST_CASE("parseManifest still accepts the largest valid int64 size") {
  const std::string sha(64, 'f');
  const Manifest m = parseManifest("x.bin|" + sha + "|9223372036854775807\n", "1.0");
  REQUIRE(m.files.size() == 1);
  CHECK(m.files[0].size == 9223372036854775807LL);

  // One more than int64 max is rejected, not wrapped into a negative size.
  CHECK(parseManifest("y.bin|" + sha + "|9223372036854775808\n", "1.0").files.empty());
}

// ---------------------------------------------------------------------------
// compareVersions
// ---------------------------------------------------------------------------

TEST_CASE("compareVersions equal versions") {
  CHECK(compareVersions("0.10.0", "0.10.0") == 0);
  CHECK(compareVersions("1", "1") == 0);
  CHECK(compareVersions("", "") == 0);
}

TEST_CASE("compareVersions ordering") {
  CHECK(compareVersions("0.9.9", "0.10.0") < 0);
  CHECK(compareVersions("0.10.0", "0.9.9") > 0);
  CHECK(compareVersions("1.2.3", "1.2.4") < 0);
  CHECK(compareVersions("2.0.0", "1.9.9") > 0);
}

TEST_CASE("compareVersions missing segments count as zero") {
  CHECK(compareVersions("1.0", "1") == 0);
  CHECK(compareVersions("1.0.0", "1") == 0);
  CHECK(compareVersions("1.0.1", "1") > 0);
  CHECK(compareVersions("1", "1.0.1") < 0);
}

TEST_CASE("compareVersions ignores non-numeric suffixes") {
  CHECK(compareVersions("0.10.0-dev", "0.10.0") == 0);
  CHECK(compareVersions("0.10.0-dev", "0.10.1") < 0);
  CHECK(compareVersions("1.2rc1", "1.2") == 0);
}

TEST_CASE("compareVersions is numeric, not lexical") {
  CHECK(compareVersions("10.0", "9.9") > 0);
  CHECK(compareVersions("0.10.0", "0.9.0") > 0);
  CHECK(compareVersions("0.2", "0.10") < 0);
}

TEST_CASE("compareVersions saturates absurd segments instead of overflowing") {
  // version.txt comes off the network: a 60-digit segment must not run
  // "v = v * 10 + digit" into signed overflow (UB).
  const std::string huge(60, '9');
  CHECK(compareVersions(huge, "1.0") > 0);
  CHECK(compareVersions("1.0", huge) < 0);
  CHECK(compareVersions(huge, huge) == 0);
  CHECK(compareVersions("1." + huge, "1." + huge) == 0);
  CHECK(compareVersions("1." + huge, "2.0") < 0);
  // 2^64 wrapped the accumulator exactly to 0, so this "version" used to compare
  // as OLDER than 1 - a corrupt version.txt could suppress an update.
  CHECK(compareVersions("18446744073709551616", "1") > 0);
  // The saturation point is above any plausible version number.
  CHECK(compareVersions("999999999999999999", "1000000") > 0);
}

// ---------------------------------------------------------------------------
// sha256File
// ---------------------------------------------------------------------------

TEST_CASE("sha256File matches the known 'abc' test vector") {
  namespace fs = std::filesystem;
  const fs::path tmp = fs::temp_directory_path() / "amu_test_sha256_abc.tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    REQUIRE(static_cast<bool>(f));
    f << "abc";
  }

  CHECK(sha256File(tmp.string()) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  std::error_code ec;
  fs::remove(tmp, ec);
}

TEST_CASE("sha256File of an empty file is the SHA-256 empty digest") {
  namespace fs = std::filesystem;
  const fs::path tmp = fs::temp_directory_path() / "amu_test_sha256_empty.tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    REQUIRE(static_cast<bool>(f));
  }

  CHECK(sha256File(tmp.string()) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  std::error_code ec;
  fs::remove(tmp, ec);
}

TEST_CASE("sha256File returns empty string for a missing file") {
  CHECK(sha256File("Z:\\definitely\\not\\here\\nope.bin").empty());
}

// ---------------------------------------------------------------------------
// splitHttpsUrl - the preview-image downloader feeds this a URL scraped from a
// Workshop page, so the rejections matter as much as the happy path.

TEST_CASE("splitHttpsUrl splits host and path") {
  const HttpsUrl u = splitHttpsUrl("https://steamuserimages-a.akamaihd.net/ugc/42/AB/");
  REQUIRE(u.ok);
  CHECK(u.host == "steamuserimages-a.akamaihd.net");
  CHECK(u.path == "/ugc/42/AB/");
}

TEST_CASE("splitHttpsUrl keeps the query string") {
  const HttpsUrl u = splitHttpsUrl("https://host.example/ugc/1/H/?imw=268&imh=151");
  REQUIRE(u.ok);
  CHECK(u.host == "host.example");
  CHECK(u.path == "/ugc/1/H/?imw=268&imh=151");
}

TEST_CASE("splitHttpsUrl defaults a missing path to /") {
  const HttpsUrl u = splitHttpsUrl("https://host.example");
  REQUIRE(u.ok);
  CHECK(u.host == "host.example");
  CHECK(u.path == "/");
}

TEST_CASE("splitHttpsUrl prefixes a bare query with /") {
  const HttpsUrl u = splitHttpsUrl("https://host.example?a=b");
  REQUIRE(u.ok);
  CHECK(u.path == "/?a=b");
}

TEST_CASE("splitHttpsUrl strips the fragment") {
  const HttpsUrl u = splitHttpsUrl("https://host.example/a.png#frag");
  REQUIRE(u.ok);
  CHECK(u.path == "/a.png");
}

TEST_CASE("splitHttpsUrl accepts an uppercase scheme") {
  CHECK(splitHttpsUrl("HTTPS://host.example/a").ok);
}

TEST_CASE("splitHttpsUrl rejects every non-https scheme") {
  CHECK_FALSE(splitHttpsUrl("http://host.example/a").ok);
  CHECK_FALSE(splitHttpsUrl("ftp://host.example/a").ok);
  CHECK_FALSE(splitHttpsUrl("file:///C:/windows/win.ini").ok);
  CHECK_FALSE(splitHttpsUrl("javascript:alert(1)").ok);
  CHECK_FALSE(splitHttpsUrl("//host.example/a").ok);
  CHECK_FALSE(splitHttpsUrl("/ugc/relative.png").ok);
  CHECK_FALSE(splitHttpsUrl("").ok);
  CHECK_FALSE(splitHttpsUrl("https://").ok);
}

TEST_CASE("splitHttpsUrl rejects userinfo and an explicit port") {
  // Both would send the request somewhere other than what the URL reads like:
  // the helper always connects to <host>:443 and sends no credentials.
  CHECK_FALSE(splitHttpsUrl("https://steamcommunity.com@evil.example/a").ok);
  CHECK_FALSE(splitHttpsUrl("https://host.example:8443/a").ok);
}

TEST_CASE("splitHttpsUrl rejects control characters and spaces") {
  CHECK_FALSE(splitHttpsUrl("https://host.example/a b.png").ok);
  CHECK_FALSE(splitHttpsUrl("https://host.example/a\r\nX-Evil: 1").ok);
  CHECK_FALSE(splitHttpsUrl("https://ho st.example/a").ok);
}
