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

#pragma pack(push, 1)
struct PacketHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint32_t sequence_id;
  uint32_t payload_length;

  std::string to_cstr() const {

    char buffer[32];
    snprintf(buffer, sizeof(buffer),
             "%05" PRIu16 "%03" PRIu8 "%03" PRIu8 "%10" PRIu32 "%10" PRIu32,
             this->magic, this->version, this->type, this->sequence_id,
             this->payload_length);

    return std::string(buffer);
  }
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

inline std::vector<uint8_t> create_packet_stream(PacketHeader &header,
                                                 const char *payload) {

  std::string char_stream = std::string(header.to_cstr() + payload).c_str();
  return std::vector<uint8_t>(char_stream.begin(), char_stream.end());
}

inline void safePrint(const std::string &msg) {
  int color_pair = 0;

  if (msg.find("[SERVER]") != std::string::npos ||
      msg.find("[ADMIN]") != std::string::npos) {
    color_pair = 1;
  } else if (msg.find("You:") == 0) {
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
