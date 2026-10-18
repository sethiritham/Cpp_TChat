#include "server_message_handler.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

HandshakeResult process_handshake_packet(ClientConnection *connection) {
  PacketHeader header;
  HandshakeResult result;

  std::memcpy(&header, connection->rx_buffer.data(), sizeof(PacketHeader));

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

  if (connection->rx_buffer.size() < total_size) {
    result.status = HandshakeStreamStatus::IncompletePacket;
    return result;
  }

  std::memcpy(payload, connection->rx_buffer.data() + sizeof(PacketHeader),
              payload_length);

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
ProcessStreamResult process_client_stream(ClientSession *session,
                                          ClientManager &client_manager) {

  ClientConnection *connection = session->connection;

  connection->last_seen = time(nullptr);

  while (connection->rx_buffer.size() >= sizeof(PacketHeader)) {
    PacketHeader header;

    std::memcpy(&header, connection->rx_buffer.data(), sizeof(PacketHeader));

    if (ntohs(header.magic) != PROTOCOL_KEY) {
      return ProcessStreamResult::InvalidProtocolError;
    }

    PacketType type = static_cast<PacketType>(header.type);
    uint32_t payload_len = ntohl(header.payload_length);
    size_t total_size = sizeof(PacketHeader) + payload_len;

    if (connection->rx_buffer.size() < total_size) {
      break;
    }

    std::string payload(connection->rx_buffer.begin() + sizeof(PacketHeader),
                        connection->rx_buffer.begin() + total_size);

    connection->rx_buffer.erase(connection->rx_buffer.begin(),
                                connection->rx_buffer.begin() + total_size);

    switch (type) {

    case PacketType::Message:
      print_message(payload, PrintKind::Client, session->username);
      break;

    case PacketType::Command:
      if (payload == "/quit") {
        client_manager.disconnect(session->connection);
        return ProcessStreamResult::ClientDisconnected;
      }
      break;

    case PacketType::Ping:
      client_manager.pong(session->connection);
      break;

    default:

      break;
    }
  }

  return ProcessStreamResult::ProcessComplete;
}
