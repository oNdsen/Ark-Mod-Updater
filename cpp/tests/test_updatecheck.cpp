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
