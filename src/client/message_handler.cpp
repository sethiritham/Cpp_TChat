#include "client/message_handler.hpp"
#include "common/protocol.hpp"

ProcessStreamResult process_data_stream(std::vector<uint8_t> &rx_buffer) {

  ProcessStreamResult result{};

  if (rx_buffer.empty()) {
    result.status = ProcessStreamStatus::BufferEmpty;
    return result;
  }

  if (rx_buffer.size() < sizeof(PacketHeader)) {
    result.status = ProcessStreamStatus::IncompletePacket;
    return result;
  }

  PacketHeader header{};

  std::memcpy(&header, rx_buffer.data(), sizeof(PacketHeader));

  if (ntohs(header.magic) != PROTOCOL_KEY) {
    result.status = ProcessStreamStatus::InvalidProtocolError;
    return result;
  }

  PacketType type = static_cast<PacketType>(header.type);

  uint32_t payload_len = ntohl(header.payload_length);

  if (payload_len > MAX_MESSAGE_PAYLOAD_SIZE) {
    result.status = ProcessStreamStatus::InvalidProtocolError;
    return result;
  }

  size_t total_size = sizeof(PacketHeader) + payload_len;

  if (rx_buffer.size() < total_size) {
    result.status = ProcessStreamStatus::IncompletePacket;
    return result;
  }

  std::string payload(rx_buffer.begin() + sizeof(PacketHeader),
                      rx_buffer.begin() + total_size);

  rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + total_size);

  switch (type) {

  case PacketType::Message: {

    if (payload.size() < 2 || payload[1] != '|') {
      result.status = ProcessStreamStatus::InvalidProtocolError;
      return result;
    }

    if (payload[0] == 'U') {

      size_t delimiter = payload.find('|', 2);

      if (delimiter == std::string::npos || delimiter == 2) {
        result.status = ProcessStreamStatus::InvalidProtocolError;
        return result;
      }

      result.sender = payload.substr(2, delimiter - 2);

      result.message = payload.substr(delimiter + 1);

      result.status = ProcessStreamStatus::PrintMessage;

      return result;
    }

    if (payload[0] == 'S') {

      result.sender = "SERVER";

      result.message = payload.substr(2);

      result.status = ProcessStreamStatus::PrintMessage;

      return result;
    }

    result.status = ProcessStreamStatus::InvalidProtocolError;
    return result;
  }

  case PacketType::Pong: {

    result.status = ProcessStreamStatus::ReceivedPong;
    return result;
  }

  case PacketType::Ack: {

    result.message = std::move(payload);

    result.status = ProcessStreamStatus::Acknowledgement;

    return result;
  }

  case PacketType::Error: {

    result.message = std::move(payload);

    result.status = ProcessStreamStatus::ErrorAck;

    return result;
  }

  default: {

    result.status = ProcessStreamStatus::InvalidProtocolError;

    return result;
  }
  }
}
