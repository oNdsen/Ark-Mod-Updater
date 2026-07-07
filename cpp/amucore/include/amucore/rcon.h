#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace amucore {

// Source-RCON packet type constants (https://developer.valvesoftware.com/wiki/Source_RCON_Protocol).
// AUTH_RESPONSE and EXECCOMMAND share the wire value 2 (they are told apart by context).
constexpr int32_t kRconTypeResponseValue = 0;  // server response to an EXECCOMMAND
constexpr int32_t kRconTypeExecCommand = 2;    // client -> server: run a command
constexpr int32_t kRconTypeAuthResponse = 2;   // server -> client: result of an auth
constexpr int32_t kRconTypeAuth = 3;           // client -> server: authenticate

// A decoded RCON packet (the size length-prefix is consumed, not stored).
struct RconPacket {
  int32_t id = 0;
  int32_t type = 0;
  std::string body;  // command/response text, without the two trailing null bytes
};

// --- Pure packet codec (no sockets, unit-testable) --------------------------

// Encode one packet: [int32 size][int32 id][int32 type][body + \0][\0], little-endian.
// size = 4 (id) + 4 (type) + body.size() + 2 (two trailing nulls). Mirrors __RCON_Build.
std::vector<uint8_t> encodePacket(int32_t id, int32_t type, const std::string& body);

// Try to decode one whole packet from the front of `buffer`. If a complete packet is
// present, returns it and sets `consumed` to the number of bytes it occupied (4 + size);
// the caller should erase that many bytes. If the buffer does not yet hold a full packet
// (partial read), returns std::nullopt and leaves `consumed` at 0 so the caller keeps
// reading. Mirrors the framing in __RCON_Read (same 10..8192 size sanity bound).
std::optional<RconPacket> tryDecodePacket(const std::vector<uint8_t>& buffer, size_t& consumed);

// --- Socket I/O (Winsock) ---------------------------------------------------

// Result of a blocking RCON operation.
enum class RconStatus {
  Ok = 0,
  ConnectFailed = 2,  // TCPConnect failed
  AuthSendFailed = 3,  // send of the auth packet failed
  AuthFailed = 4,      // wrong password / auth timeout (server returned id == -1)
  CommandSendFailed = 5,  // send of the command packet failed
};

// Native Source-RCON client. One instance == one TCP connection. Not copyable.
// Usage mirrors _RCON_Command: connect(), authenticate(), then command().
class RconClient {
 public:
  explicit RconClient(int timeoutMs = 5000);
  ~RconClient();

  RconClient(const RconClient&) = delete;
  RconClient& operator=(const RconClient&) = delete;

  // Open a TCP connection to host:port. Returns Ok or ConnectFailed.
  RconStatus connect(const std::string& host, uint16_t port);

  // Send the auth packet and wait for the AUTH_RESPONSE. Returns Ok on success,
  // AuthSendFailed if the send failed, or AuthFailed on a wrong password / timeout.
  RconStatus authenticate(const std::string& password);

  // Send one command and return the server's response body in `response`.
  // Returns Ok (response filled, possibly empty if none arrived) or CommandSendFailed.
  RconStatus command(const std::string& cmd, std::string& response);

  // Convenience: connect + authenticate + command in one call, matching _RCON_Command.
  // On success `response` holds the server reply. Returns the first non-Ok status.
  RconStatus runCommand(const std::string& host, uint16_t port, const std::string& password,
                        const std::string& cmd, std::string& response);

  void close();

 private:
  // Send all bytes; false on socket error.
  bool sendAll(const std::vector<uint8_t>& bytes);
  // Read one whole packet (blocking, respecting the timeout). false on error/timeout.
  bool readPacket(RconPacket& out);

  int timeoutMs_;
  uintptr_t sock_;             // SOCKET (INVALID_SOCKET when not connected)
  bool wsaStarted_ = false;    // whether this instance called WSAStartup
  std::vector<uint8_t> rxBuf_;  // leftover bytes between packet reads (TCP fragmentation)
};

}  // namespace amucore
