/**
 * @file chat2.hpp
 * @brief Contains functions that will be used by all participants of the chat
 */

#ifndef CHAT2_HPP
#define CHAT2_HPP

#include "inttypes.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
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
 *
 */
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

inline WINDOW *chatBorder, *chatWin;
inline WINDOW *inputBorder, *inputWin;
inline int screenHeight, screenWidth;

/**
 * @brief Initilization of the Ncurses window
 * noecho
 */
inline void setupNcurses() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, true);

  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_RED, -1);
  }

  getmaxyx(stdscr, screenHeight, screenWidth);

  chatBorder = newwin(screenHeight - 3, screenWidth, 0, 0);
  chatWin = newwin(screenHeight - 5, screenWidth - 2, 1, 1);
  inputBorder = newwin(3, screenWidth, screenHeight - 3, 0);
  inputWin = newwin(1, screenWidth - 2, screenHeight - 2, 1);

  scrollok(chatWin, true);
  scrollok(inputWin, true);
  keypad(inputWin, true);

  box(chatBorder, 0, 0);
  mvwprintw(chatBorder, 0, 1, "[ CHAT ROOM ]");

  box(inputBorder, 0, 0);
  mvwprintw(inputBorder, 0, 1, "[ MESSAGE ]");

  wrefresh(chatBorder);
  wrefresh(chatWin);
  wrefresh(inputBorder);
  wrefresh(inputWin);
}

inline void cleanupNcurses() {
  delwin(chatWin);
  delwin(inputWin);
  delwin(chatBorder);
  delwin(inputBorder);
  endwin();
}

/**
 * @brief Convert the message payload to packet
 * Combines the Packet header and the payload, converts it to a u8 vector
 * @param type The type of the packet to be created
 * @param payload Message to be packetized
 * @return Formatted packet
 */
inline std::vector<uint8_t> create_packet_stream(const uint8_t &type,
                                                 const std::string &payload) {

  PacketHeader header{};

  header.type = type;
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

/**
 * @brief Simple function to display the message in the chat window
 * Sets color to the text depending on who is sending it
 * @param msg Message to be displayed
 * @param italics set the message format italics, default false
 * @param bold set the message format bold, default false
 */
inline void safePrint(const std::string &msg, bool italics = false,
                      bool bold = false) {
  int color_pair = 0;

  if (msg.find("[SERVER]") != std::string::npos ||
      msg.find("[ADMIN]") != std::string::npos) {
    color_pair = 1;
  } else if (msg.find("[YOU]") == 0) {
    color_pair = 2;
  } else if (msg.find("Error") != std::string::npos ||
             msg.find("Quitting") != std::string::npos) {
    color_pair = 3;
  }

  if (color_pair > 0)
    wattron(chatWin, COLOR_PAIR(color_pair));

  if (italics)
    attron(A_ITALIC);
  else
    attroff(A_ITALIC);

  wprintw(chatWin, "%s\n", msg.c_str());

  if (color_pair > 0)
    wattroff(chatWin, COLOR_PAIR(color_pair));

  wrefresh(chatWin);
  wrefresh(inputWin);
}

#endif
