#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "amucore/rcon.h"

using namespace amucore;

namespace {

int32_t i32LE(const std::vector<uint8_t>& b, size_t off) {
  return static_cast<int32_t>(static_cast<uint32_t>(b[off]) |
                              (static_cast<uint32_t>(b[off + 1]) << 8) |
                              (static_cast<uint32_t>(b[off + 2]) << 16) |
                              (static_cast<uint32_t>(b[off + 3]) << 24));
}

}  // namespace

TEST_CASE("encodePacket lays out size/id/type/body + two nulls, little-endian") {
  const std::string body = "saveworld";
  const auto pkt = encodePacket(2, kRconTypeExecCommand, body);

  // Total bytes = 4 (size prefix) + size.  size = 4 + 4 + body + 2.
  const int32_t size = i32LE(pkt, 0);
  CHECK(size == 4 + 4 + static_cast<int32_t>(body.size()) + 2);
  CHECK(pkt.size() == static_cast<size_t>(size) + 4);

  CHECK(i32LE(pkt, 4) == 2);                      // id
  CHECK(i32LE(pkt, 8) == kRconTypeExecCommand);   // type

  // body starts at offset 12 and is followed by exactly two null bytes.
  const std::string got(pkt.begin() + 12, pkt.begin() + 12 + body.size());
  CHECK(got == body);
  CHECK(pkt[pkt.size() - 2] == 0x00);
  CHECK(pkt[pkt.size() - 1] == 0x00);
}

TEST_CASE("empty body still encodes the two trailing nulls (size == 10)") {
  const auto pkt = encodePacket(1, kRconTypeAuth, "");
  CHECK(i32LE(pkt, 0) == 10);
  CHECK(pkt.size() == 14);
  CHECK(pkt[12] == 0x00);
  CHECK(pkt[13] == 0x00);
}

TEST_CASE("encode -> decode round-trips id, type and body") {
  const auto wire = encodePacket(7, kRconTypeResponseValue, "hello world");
  size_t consumed = 0;
  auto pkt = tryDecodePacket(wire, consumed);
  REQUIRE(pkt.has_value());
  CHECK(consumed == wire.size());
  CHECK(pkt->id == 7);
  CHECK(pkt->type == kRconTypeResponseValue);
  CHECK(pkt->body == "hello world");
}

TEST_CASE("auth failure is detectable: AUTH_RESPONSE with id == -1") {
  // Server rejects the password by echoing an AUTH_RESPONSE (type 2) with id -1.
  const auto wire = encodePacket(-1, kRconTypeAuthResponse, "");
  size_t consumed = 0;
  auto pkt = tryDecodePacket(wire, consumed);
  REQUIRE(pkt.has_value());
  CHECK(pkt->type == kRconTypeAuthResponse);
  CHECK(pkt->id == -1);  // caller treats id == -1 as wrong password
}

TEST_CASE("successful auth: AUTH_RESPONSE with the original request id") {
  const auto wire = encodePacket(1, kRconTypeAuthResponse, "");
  size_t consumed = 0;
  auto pkt = tryDecodePacket(wire, consumed);
  REQUIRE(pkt.has_value());
  CHECK(pkt->type == kRconTypeAuthResponse);
  CHECK(pkt->id != -1);
}

TEST_CASE("partial buffer (only the size field) decodes to nullopt without consuming") {
  auto wire = encodePacket(3, kRconTypeResponseValue, "abc");
  const std::vector<uint8_t> partial(wire.begin(), wire.begin() + 4);  // just the size prefix
  size_t consumed = 123;
  auto pkt = tryDecodePacket(partial, consumed);
  CHECK_FALSE(pkt.has_value());
  CHECK(consumed == 0);
}

TEST_CASE("body reassembled from two partial reads") {
  // Simulate TCP fragmentation: the full packet arrives in two chunks. The first chunk
  // is not a complete packet (nullopt); after appending the second, it decodes fully.
  const std::string body = "Broadcast sent to all players on the server";
  const auto full = encodePacket(2, kRconTypeResponseValue, body);

  const size_t split = 9;  // mid-header split, deliberately not on a field boundary
  std::vector<uint8_t> rx(full.begin(), full.begin() + split);

  size_t consumed = 0;
  CHECK_FALSE(tryDecodePacket(rx, consumed).has_value());
  CHECK(consumed == 0);

  // second read appends the remainder
  rx.insert(rx.end(), full.begin() + split, full.end());
  auto pkt = tryDecodePacket(rx, consumed);
  REQUIRE(pkt.has_value());
  CHECK(consumed == full.size());
  CHECK(pkt->id == 2);
  CHECK(pkt->body == body);
}

TEST_CASE("two back-to-back packets in one buffer decode one at a time via consumed") {
  auto a = encodePacket(10, kRconTypeResponseValue, "first");
  auto b = encodePacket(11, kRconTypeResponseValue, "second");
  std::vector<uint8_t> rx = a;
  rx.insert(rx.end(), b.begin(), b.end());

  size_t consumed = 0;
  auto p1 = tryDecodePacket(rx, consumed);
  REQUIRE(p1.has_value());
  CHECK(p1->body == "first");
  CHECK(consumed == a.size());
  rx.erase(rx.begin(), rx.begin() + consumed);

  auto p2 = tryDecodePacket(rx, consumed);
  REQUIRE(p2.has_value());
  CHECK(p2->body == "second");
  CHECK(consumed == b.size());
  rx.erase(rx.begin(), rx.begin() + consumed);

  CHECK(rx.empty());
}

TEST_CASE("out-of-range size field is rejected by the sanity bound") {
  // size < 10 is impossible for a valid packet (min is 10 for an empty body).
  std::vector<uint8_t> bad = {0x05, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  size_t consumed = 0;
  CHECK_FALSE(tryDecodePacket(bad, consumed).has_value());
  CHECK(consumed == 0);
}

TEST_CASE("body stops at an embedded null just like the AutoIt char[] read") {
  // Manually build a packet whose declared body region contains an interior null.
  std::string realBody = "ok";
  // wire: size, id, type, 'o','k', '\0', extra, then the two framing nulls
  std::vector<uint8_t> wire;
  auto push32 = [&](int32_t v) {
    wire.push_back(static_cast<uint8_t>(v & 0xFF));
    wire.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    wire.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    wire.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
  };
  // body-region bytes (between header and the trailing 2 nulls): 'o','k','\0','X'
  const int bodyRegion = 4;  // o k \0 X
  const int32_t size = 4 + 4 + bodyRegion + 2;
  push32(size);
  push32(5);
  push32(kRconTypeResponseValue);
  wire.push_back('o');
  wire.push_back('k');
  wire.push_back(0x00);
  wire.push_back('X');
  wire.push_back(0x00);
  wire.push_back(0x00);

  size_t consumed = 0;
  auto pkt = tryDecodePacket(wire, consumed);
  REQUIRE(pkt.has_value());
  CHECK(pkt->body == "ok");  // stops at the first null, "X" is dropped
  CHECK(consumed == wire.size());
}
