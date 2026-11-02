#include "common/protocol.hpp"
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
#include <vector>

enum class ProcessStreamResult {
  BufferEmpty,
  IncompletePacket,
  InvalidProtocolError,
  ProcessComplete,
  DisconnectClient,
  ReceivedPong,
  Acknowledgement,
  ErrorAck,
  PrintMessage
};

ProcessStreamResult process_client_stream(std::vector<uint8_t> &rx_buffer,
                                          std::string &message) {

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
      message = payload;
      return ProcessStreamResult::PrintMessage;

    case PacketType::Pong:
      return ProcessStreamResult::ReceivedPong;

    case PacketType::Ack:
      message = payload;
      return ProcessStreamResult::Acknowledgement;

    case PacketType::Error:
      return ProcessStreamResult::ErrorAck;

    default:
      break;
    }
  }

  return ProcessStreamResult::ProcessComplete;
}
