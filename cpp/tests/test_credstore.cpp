#include <doctest/doctest.h>

#include <string>

#include "amucore/credstore.h"

using namespace amucore;

TEST_CASE("protect then unprotect round-trips a credential") {
  const std::string plain = "hunter2";
  const std::string stored = protect(plain);
  CHECK(stored.rfind("DPAPI:0x", 0) == 0);  // starts with the stored-form prefix
  CHECK(unprotect(stored) == plain);
}

TEST_CASE("protect of empty string returns empty; round-trips") {
  CHECK(protect("") == "");
  CHECK(unprotect("") == "");
}

TEST_CASE("round-trips UTF-8 / non-ASCII bytes") {
  const std::string plain = "P\xC3\xA4ssw\xC3\xB6rd!\xE2\x9C\x93";  // "Pässwörd!✓"
  const std::string stored = protect(plain);
  CHECK(unprotect(stored) == plain);
}

TEST_CASE("unprotect passes legacy plaintext through unchanged") {
  CHECK(unprotect("myOldPlainPassword") == "myOldPlainPassword");
  CHECK(unprotect("0xNotReallyDpapi") == "0xNotReallyDpapi");
}

TEST_CASE("unprotect of garbage DPAPI blob does not crash, returns input") {
  const std::string garbage = "DPAPI:0xDEADBEEF";
  CHECK(unprotect(garbage) == garbage);
}

TEST_CASE("unprotect of malformed hex does not crash, returns input") {
  CHECK(unprotect("DPAPI:0xZZ") == "DPAPI:0xZZ");   // non-hex digit
  CHECK(unprotect("DPAPI:0xABC") == "DPAPI:0xABC");  // odd digit count
  CHECK(unprotect("DPAPI:") == "DPAPI:");            // empty payload
}
