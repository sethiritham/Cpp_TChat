#include "server_message_handler.hpp"
#include "protocol.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

HandshakeResult process_handshake_packet(std::vector<uint8_t> &rx_buffer) {
  PacketHeader header;
  HandshakeResult result;
  result.request.kind = HandshakeKind::Error;

  if (rx_buffer.size() < sizeof(PacketHeader)) {
    result.status = HandshakeStreamStatus::IncompletePacket;
    return result;
  }

  std::memcpy(&header, rx_buffer.data(), sizeof(PacketHeader));

  if (ntohs(header.magic) != PROTOCOL_KEY) {
    result.status = HandshakeStreamStatus::InvalidProtocolError;
    return result;
  }

  char payload[512];
  int payload_length = ntohl(header.payload_length);

  size_t total_size = payload_length + sizeof(PacketHeader);

  if (total_size > MAX_HANDSHAKE_PAYLOAD || payload_length == 0) {
    result.status = HandshakeStreamStatus::InvalidAuthPacket;
    result.request.kind = HandshakeKind::Error;
    return result;
  }

  if (rx_buffer.size() < total_size) {
    result.status = HandshakeStreamStatus::IncompletePacket;
    return result;
  }

  std::memcpy(payload, rx_buffer.data() + sizeof(PacketHeader), payload_length);

  rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + total_size);

  payload[payload_length] = '\0';

  std::string message = std::string(payload);

  size_t delimeter = message.find("|");

  if (delimeter == std::string::npos) {
    result.status = HandshakeStreamStatus::InvalidAuthPacket;
    result.request.kind = HandshakeKind::Error;
    return result;
  }

  std::string username = message.substr(0, delimeter);
  std::string password = message.substr(delimeter + 1);

  switch (static_cast<PacketType>(header.type)) {
  case PacketType::Login: {
    result.request.kind = HandshakeKind::Login;
    result.request.username = username;
    result.request.password = password;
    result.status = HandshakeStreamStatus::ValidAuthPacket;
    break;
  }
  case PacketType::Reconnnect:
    result.request.kind = HandshakeKind::Reconnect;
    result.request.username = username;
    result.request.token = password;
    result.status = HandshakeStreamStatus::ValidAuthPacket;
    break;

  case PacketType::Register:
    result.request.kind = HandshakeKind::Register;
    result.request.username = username;
    result.request.password = password;
    result.status = HandshakeStreamStatus::ValidAuthPacket;
    break;

  default:
    result.request.kind = HandshakeKind::Error;
    result.status = HandshakeStreamStatus::InvalidAuthPacket;
    break;
  }

  return result;
}
/**
 * @brief Processes the receive buffer of the client session and broadcasts it
 * after packetizing it
 *
 * @param session session from whom server received the data
 */
ProcessStreamResult process_client_stream(std::vector<uint8_t> &rx_buffer) {

  while (rx_buffer.size() >= sizeof(PacketHeader)) {
    PacketHeader header;

    std::memcpy(&header, rx_buffer.data(), sizeof(PacketHeader));

    if (ntohs(header.magic) != PROTOCOL_KEY) {
      return ProcessStreamResult::InvalidProtocolError;
    }

    PacketType type = static_cast<PacketType>(header.type);
    uint32_t payload_len = ntohl(header.payload_length);
    size_t total_size = sizeof(PacketHeader) + payload_len;

    if (rx_buffer.size() < total_size) {
      return ProcessStreamResult::IncompletePacket;
    }

    std::string payload(rx_buffer.begin() + sizeof(PacketHeader),
                        rx_buffer.begin() + total_size);

    rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + total_size);

    switch (type) {

    case PacketType::Message:
      return ProcessStreamResult::PrintMessage;

    case PacketType::Command:
      if (payload == "/quit") {
        return ProcessStreamResult::DisconnectClient;
      }

    case PacketType::Ping:
      return ProcessStreamResult::SendPong;

    default:
      break;
    }
  }

  return ProcessStreamResult::ProcessComplete;
}
