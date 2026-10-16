#include "server_message_handler.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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
      if (payload.find("/quit") != std::string::npos) {
        client_manager.disconnect(session->connection);
      }
      break;
    case PacketType::Ping:
      client_manager.pong(session->connection);
      break;
    case PacketType::Auth: {
      size_t delimeter = payload.find("|");
      if (delimeter == std::string::npos) {
        break;
      }

      std::string username = payload.substr(0, delimeter);
      std::string password = payload.substr(delimeter + 1);
      client_manager.authorize_client(username, password);
      break;
    }
    case PacketType::Register: {
      size_t delimeter = payload.find("|");

      if (delimeter == std::string::npos) {
        break;
      }

      std::string username = payload.substr(0, delimeter);
      std::string password = payload.substr(delimeter + 1);

      client_manager.register_client(username, password);

      break;
    }

    default:

      break;
    }
  }

  return ProcessStreamResult::BufferEmpty;
}
