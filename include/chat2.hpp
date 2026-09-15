#ifndef CHAT2_HPP
#define CHAT2_HPP

#include "inttypes.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <ncurses.h>
#include <string>
#include <vector>

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 32;
constexpr uint16_t PROTOCOL_KEY = 0x4354; // "CT"

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
struct PacketHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint32_t sequence_id;
  uint32_t payload_length;
};
#pragma pack(pop)

inline WINDOW *chatBorder, *chatWin;
inline WINDOW *inputBorder, *inputWin;
inline int screenHeight, screenWidth;

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

inline void safePrint(const std::string &msg) {
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

  wprintw(chatWin, "%s\n", msg.c_str());

  if (color_pair > 0)
    wattroff(chatWin, COLOR_PAIR(color_pair));

  wrefresh(chatWin);
  wrefresh(inputWin);
}

#endif
