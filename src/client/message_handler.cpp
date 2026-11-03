#include "client/message_handler.hpp"
#include "common/protocol.hpp"

ProcessStreamResult process_data_stream(std::vector<uint8_t> &rx_buffer,
                                        std::string &message) {
  message.clear();

  if (rx_buffer.empty())
    return ProcessStreamResult::BufferEmpty;

  if (rx_buffer.size() < sizeof(PacketHeader))
    return ProcessStreamResult::IncompletePacket;

  PacketHeader header{};

  std::memcpy(&header, rx_buffer.data(), sizeof(PacketHeader));

  if (ntohs(header.magic) != PROTOCOL_KEY)
    return ProcessStreamResult::InvalidProtocolError;

  PacketType type = static_cast<PacketType>(header.type);

  uint32_t payload_len = ntohl(header.payload_length);

  if (payload_len > MAX_MESSAGE_PAYLOAD_SIZE)
    return ProcessStreamResult::InvalidProtocolError;

  size_t total_size = sizeof(PacketHeader) + payload_len;

  if (rx_buffer.size() < total_size)
    return ProcessStreamResult::IncompletePacket;

  std::string payload(rx_buffer.begin() + sizeof(PacketHeader),
                      rx_buffer.begin() + total_size);

  rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + total_size);

  switch (type) {

  case PacketType::Message:
    message = std::move(payload);
    return ProcessStreamResult::PrintMessage;

  case PacketType::Pong:
    return ProcessStreamResult::ReceivedPong;

  case PacketType::Ack:
    message = std::move(payload);
    return ProcessStreamResult::Acknowledgement;

  case PacketType::Error:
    message = std::move(payload);
    return ProcessStreamResult::ErrorAck;

  default:
    return ProcessStreamResult::InvalidProtocolError;
  }
}
