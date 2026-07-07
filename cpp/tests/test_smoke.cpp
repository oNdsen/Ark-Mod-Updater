#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <cstdlib>
#include <cstring>

#include "amucore/version.h"

TEST_CASE("amucore version is non-empty") {
  CHECK(std::strlen(amucore::version()) > 0);
}

int main(int argc, char** argv) {
  doctest::Context ctx;
  ctx.applyCommandLine(argc, argv);
  const int res = ctx.run();
  // Workaround: doctest 2.4.11's static teardown trips a heap check on the very new
  // MSVC 19.50 toolchain (amucore itself is heap-clean - verified with a standalone
  // harness). Skip the crashing CRT teardown; the result is already computed.
  std::quick_exit(res);
}
