#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <vector>

#include "amucore/zunpack.h"

using namespace amucore;

static std::vector<uint8_t> readFileZ(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

TEST_CASE(".z unpack matches the AutoIt golden output byte-for-byte") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto z = readFileZ(dir + "/sample.z");
  const auto golden = readFileZ(dir + "/sample.unpacked");
  REQUIRE(z.size() == 442);
  REQUIRE(golden.size() == 858);

  int err = -1;
  const auto out = unpackZ(z, &err);
  CHECK(err == 0);
  CHECK(out.size() == golden.size());
  CHECK(out == golden);
}

TEST_CASE(".z unpack rejects a bad signature") {
  std::vector<uint8_t> bad(64, 0);
  int err = 0;
  const auto out = unpackZ(bad, &err);
  CHECK(out.empty());
  CHECK(err == 2);
}
