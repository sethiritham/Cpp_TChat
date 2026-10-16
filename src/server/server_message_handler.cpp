#include "server_message_handler.hpp"

enum class ProcessStreamResult { BufferEmpty, InvalidProtocolError };

/**
 * @brief Processes the receive buffer of the client session and broadcasts it
 * after packetizing it
 *
 * @param session session from whom server received the data
 */
void process_client_stream(ClientConnection *connection) {

  connection->last_seen = time(nullptr);

  while (connection->rx_buffer.size() >= sizeof(PacketHeader)) {
    PacketHeader header;

    std::memcpy(&header, connection->rx_buffer.data(), sizeof(PacketHeader));

    if (ntohs(header.magic) != PROTOCOL_KEY) {
      return;
    }

    PacketType type = static_cast<PacketType>(header.type);
    uint32_t payload_len = ntohl(header.payload_length);
    size_t total_size = sizeof(PacketHeader) + payload_len;

    if (connection->rx_buffer.size() < total_size) {
      break;
    }

    switch (type) {

    case PacketType::Message:
      // print the message
      // break;
    case PacketType::Command:
      // client quits (only command for now)
      break;
    case PacketType::Ping:
      // Pong
      break;
    case PacketType::Auth:
      // Login
      break;
    case PacketType::Register:
      // add new client
      break;
    default:

      break;
    }
  }
}
