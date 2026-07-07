#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <vector>

#include "amucore/mod_writer.h"

using namespace amucore;

static std::vector<uint8_t> readFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

TEST_CASE(".mod is byte-identical to the AutoIt golden for mod 632091170") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto modinfo = readFile(dir + "/632091170_mod.info");
  const auto modmeta = readFile(dir + "/632091170_modmeta.info");
  const auto golden = readFile(dir + "/632091170.mod");
  REQUIRE(golden.size() == 276);

  const auto parsed = parseModInfo(modinfo);
  CHECK(parsed.maps.size() == 1);

  const auto out = buildModFile(632091170ull, parsed, modmeta);
  CHECK(out.size() == golden.size());
  CHECK(out == golden);
}

TEST_CASE("high mod id (> 2^31) writes a correct 8-byte LE id") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto modinfo = readFile(dir + "/632091170_mod.info");
  const auto modmeta = readFile(dir + "/632091170_modmeta.info");
  const auto parsed = parseModInfo(modinfo);

  const uint64_t id = 3000000000ull;  // > 2^31: the case that corrupted the old writer
  const auto out = buildModFile(id, parsed, modmeta);
  CHECK(out.size() == 276);
  uint64_t got = 0;
  for (int i = 0; i < 8; ++i) got |= static_cast<uint64_t>(out[i]) << (8 * i);
  CHECK(got == id);
}
