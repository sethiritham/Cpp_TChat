
#pragma once

#include "protocol.hpp"
#include "ui.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

enum class ProcessStreamResult {
  BufferEmpty,
  IncompletePacket,
  InvalidProtocolError,
  ProcessComplete,
  DisconnectClient,
  SendPong,
  PrintMessage
};

enum class HandshakeStreamStatus {
  InvalidAuthPacket,
  ValidAuthPacket,
  InvalidProtocolError,
  IncompletePacket,
};

enum class HandshakeKind { Login, Register, Reconnect, Error };

constexpr uint32_t MAX_HANDSHAKE_PAYLOAD = 512;

struct HandshakeRequest {
  HandshakeKind kind;
  std::string username;
  std::string password;

  std::string token;

  HandshakeRequest() {
    kind = HandshakeKind::Error;
    username = "";
    password = "";
  }
};

struct HandshakeResult {
  HandshakeStreamStatus status;
  HandshakeRequest request;
};
/**
 * @brief Processes the receive buffer of the client session and broadcasts it
 * after packetizing it
 *
 * @param session session from whom server received the data
 */
ProcessStreamResult process_client_stream(std::vector<uint8_t> &rx_buffer);

HandshakeResult process_handshake_packet(std::vector<uint8_t> &rx_buffer);
