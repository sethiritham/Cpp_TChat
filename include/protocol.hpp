#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <inttypes.h>
#include <ncurses.h>
#include <string>
#include <vector>
/**
 * @brief PORT number of the network
 */
constexpr int PORT = 8080;

/**
 * @brief Number of possible non-blocking events
 */
constexpr int MAX_EVENTS = 32;

/**
 * @brief unique protocol key contained in every packet header for packet
 * authorization
 */
constexpr uint16_t PROTOCOL_KEY = 0x4354;

/**
 * TYPES:
 * - FILE_META = 0x00
 * - FILE_CHUNK = 0x01
 * - MESSAGE = 0x02
 * - CMND = 0x03
 * - ACK = 0x04
 * - PRESENCE = 0x05
 * - POLL = 0x06
 * - PING = 0x07
 * - AUTH = 0x08
 * - RGSTR = 0x09
 */

enum class PacketType : std::uint8_t {
  FileMeta = 0x00,
  FileChunk = 0x01,
  Message = 0x02,
  Command = 0x03,
  Ack = 0x04,
  Presence = 0x05,
  Pong = 0x06,
  Ping = 0x07,
  Auth = 0x08,
  Register = 0x09
};

#pragma pack(push, 1)
/**
 * @brief Meta data of the packet
 * @param magic :
 * @param version :
 * @param type :
 * @param sequence_id :
 * @param payload_length :
 * */

/**
 * @class PacketHeader
 * @brief Meta data of the packet
 * */

struct PacketHeader {

  uint16_t magic;  ///< The unique protocol for the chat
  uint8_t version; ///< The IP version
  uint8_t
      type; ///< unique u8 number representing the type of payload in the packet
  uint32_t sequence_id;    ///< Sequential number assigned to all packets
                           ///< transferred in the protocol
  uint32_t payload_length; ///< Length of the payload
};
#pragma pack(pop)

std::vector<uint8_t> create_packet_stream(const PacketType &type,
                                          const std::string &payload);
