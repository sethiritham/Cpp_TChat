#include "protocol.hpp"

std::vector<uint8_t> create_packet_stream(const PacketType &type,
                                          const std::string &payload) {

  PacketHeader header{};

  header.type = static_cast<uint8_t>(type);
  header.magic = htons(PROTOCOL_KEY);
  header.sequence_id = htonl(1);
  header.payload_length = htonl(payload.length());

  std::vector<uint8_t> packet(sizeof(PacketHeader) + payload.size());

  std::memcpy(packet.data(), &header, sizeof(PacketHeader));

  if (!payload.empty()) {
    std::memcpy(packet.data() + sizeof(PacketHeader), payload.data(),
                payload.size());
  }

  return packet;
}
