#include "amucore/rcon.h"

// Winsock must come before <windows.h>; define lean macros to keep the include light.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstring>

namespace amucore {

namespace {

void putI32LE(std::vector<uint8_t>& o, int32_t v) {
  const uint32_t u = static_cast<uint32_t>(v);
  o.push_back(static_cast<uint8_t>(u & 0xFF));
  o.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
  o.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
  o.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
}

int32_t getI32LE(const std::vector<uint8_t>& b, size_t off) {
  return static_cast<int32_t>(static_cast<uint32_t>(b[off]) |
                              (static_cast<uint32_t>(b[off + 1]) << 8) |
                              (static_cast<uint32_t>(b[off + 2]) << 16) |
                              (static_cast<uint32_t>(b[off + 3]) << 24));
}

}  // namespace

// --- Pure packet codec ------------------------------------------------------

std::vector<uint8_t> encodePacket(int32_t id, int32_t type, const std::string& body) {
  // size = id(4) + type(4) + body + two trailing nulls.
  const int32_t size = 4 + 4 + static_cast<int32_t>(body.size()) + 2;
  std::vector<uint8_t> o;
  o.reserve(static_cast<size_t>(size) + 4);
  putI32LE(o, size);
  putI32LE(o, id);
  putI32LE(o, type);
  o.insert(o.end(), body.begin(), body.end());
  o.push_back(0x00);  // body terminator
  o.push_back(0x00);  // empty-string terminator
  return o;
}

std::optional<RconPacket> tryDecodePacket(const std::vector<uint8_t>& buffer, size_t& consumed) {
  consumed = 0;
  if (buffer.size() < 4) return std::nullopt;  // not even the size field yet

  const int32_t size = getI32LE(buffer, 0);
  if (size < 10 || size > 8192) return std::nullopt;  // sanity bound (matches AutoIt)

  const size_t total = 4 + static_cast<size_t>(size);
  if (buffer.size() < total) return std::nullopt;  // body not fully arrived yet

  RconPacket pkt;
  pkt.id = getI32LE(buffer, 4);
  pkt.type = getI32LE(buffer, 8);

  // Body spans bytes [12 .. total), minus the two trailing null bytes. AutoIt reads it
  // as a C string, so stop at the first embedded null just like DllStructGetData(char[]).
  const size_t bodyStart = 12;
  const size_t bodyEnd = total - 2;  // strip the two nulls
  for (size_t i = bodyStart; i < bodyEnd; ++i) {
    if (buffer[i] == 0x00) break;
    pkt.body.push_back(static_cast<char>(buffer[i]));
  }

  consumed = total;
  return pkt;
}

// --- Socket I/O -------------------------------------------------------------

RconClient::RconClient(int timeoutMs)
    : timeoutMs_(timeoutMs), sock_(static_cast<uintptr_t>(INVALID_SOCKET)) {}

RconClient::~RconClient() {
  close();
  if (wsaStarted_) {
    WSACleanup();
    wsaStarted_ = false;
  }
}

RconStatus RconClient::connect(const std::string& host, uint16_t port) {
  if (!wsaStarted_) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return RconStatus::ConnectFailed;
    wsaStarted_ = true;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  char portStr[16];
  std::snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port));

  addrinfo* result = nullptr;
  if (getaddrinfo(host.c_str(), portStr, &hints, &result) != 0 || result == nullptr) {
    return RconStatus::ConnectFailed;
  }

  SOCKET s = INVALID_SOCKET;
  for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
    s = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (s == INVALID_SOCKET) continue;
    if (::connect(s, ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0) break;
    ::closesocket(s);
    s = INVALID_SOCKET;
  }
  freeaddrinfo(result);

  if (s == INVALID_SOCKET) return RconStatus::ConnectFailed;

  // Give recv() a timeout so readPacket() cannot block forever.
  DWORD tv = static_cast<DWORD>(timeoutMs_);
  ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
  ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

  sock_ = static_cast<uintptr_t>(s);
  rxBuf_.clear();
  return RconStatus::Ok;
}

bool RconClient::sendAll(const std::vector<uint8_t>& bytes) {
  const SOCKET s = static_cast<SOCKET>(sock_);
  if (s == INVALID_SOCKET) return false;
  size_t off = 0;
  while (off < bytes.size()) {
    const int n = ::send(s, reinterpret_cast<const char*>(bytes.data() + off),
                         static_cast<int>(bytes.size() - off), 0);
    if (n == SOCKET_ERROR || n <= 0) return false;
    off += static_cast<size_t>(n);
  }
  return true;
}

bool RconClient::readPacket(RconPacket& out) {
  const SOCKET s = static_cast<SOCKET>(sock_);
  if (s == INVALID_SOCKET) return false;

  using clock = std::chrono::steady_clock;
  const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs_);

  for (;;) {
    // Do we already have a whole packet buffered from a previous recv?
    size_t consumed = 0;
    if (auto pkt = tryDecodePacket(rxBuf_, consumed)) {
      rxBuf_.erase(rxBuf_.begin(), rxBuf_.begin() + consumed);
      out = std::move(*pkt);
      return true;
    }

    if (clock::now() >= deadline) return false;

    char chunk[512];
    const int n = ::recv(s, chunk, sizeof(chunk), 0);
    if (n > 0) {
      rxBuf_.insert(rxBuf_.end(), chunk, chunk + n);
      continue;  // try to decode again with the new bytes
    }
    if (n == 0) return false;  // peer closed

    // n == SOCKET_ERROR: a WSAETIMEDOUT means our SO_RCVTIMEO fired; loop and re-check
    // the overall deadline. Any other error is fatal.
    if (WSAGetLastError() == WSAETIMEDOUT) continue;
    return false;
  }
}

RconStatus RconClient::authenticate(const std::string& password) {
  if (!sendAll(encodePacket(1, kRconTypeAuth, password))) return RconStatus::AuthSendFailed;

  // Wait for the AUTH_RESPONSE. Some servers first echo an empty RESPONSE_VALUE (type 0);
  // skip those and read on until the auth response (type 2) arrives, matching the
  // AutoIt loop that only reacts to $RCON_TYPE_AUTH_RESPONSE.
  using clock = std::chrono::steady_clock;
  const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs_);
  while (clock::now() < deadline) {
    RconPacket pkt;
    if (!readPacket(pkt)) break;
    if (pkt.type == kRconTypeAuthResponse) {
      // id == -1 means the password was rejected.
      return (pkt.id != -1) ? RconStatus::Ok : RconStatus::AuthFailed;
    }
  }
  return RconStatus::AuthFailed;
}

RconStatus RconClient::command(const std::string& cmd, std::string& response) {
  response.clear();
  if (!sendAll(encodePacket(2, kRconTypeExecCommand, cmd))) return RconStatus::CommandSendFailed;

  RconPacket pkt;
  if (readPacket(pkt)) response = pkt.body;
  return RconStatus::Ok;
}

RconStatus RconClient::runCommand(const std::string& host, uint16_t port,
                                  const std::string& password, const std::string& cmd,
                                  std::string& response) {
  response.clear();
  RconStatus st = connect(host, port);
  if (st != RconStatus::Ok) return st;
  st = authenticate(password);
  if (st != RconStatus::Ok) {
    close();
    return st;
  }
  st = command(cmd, response);
  close();
  return st;
}

void RconClient::close() {
  const SOCKET s = static_cast<SOCKET>(sock_);
  if (s != INVALID_SOCKET) {
    ::closesocket(s);
    sock_ = static_cast<uintptr_t>(INVALID_SOCKET);
  }
  rxBuf_.clear();
}

}  // namespace amucore
