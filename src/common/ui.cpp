#include "common/ui.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>

std::mutex consoleLock;
std::map<std::string, int> clientSockets;
std::mutex clientMutex;
WINDOW *chatBorder = nullptr, *chatWin = nullptr;
WINDOW *inputBorder = nullptr, *inputWin = nullptr;
int screenHeight = 0, screenWidth = 0;

namespace {

bool colors_ready = false;
short background_color = COLOR_BLACK;

std::unordered_map<std::string, short> user_pairs;
short next_pair = 4;
std::size_t next_palette = 0;

constexpr std::array<short, 32> extended_palette = {
    202, 208, 214, 220, 226, 190, 154, 118, 82,  48, 51, 45, 33, 69,  99,  129,
    165, 201, 198, 166, 172, 178, 184, 148, 112, 76, 43, 37, 75, 111, 147, 183};

constexpr std::array<short, 4> basic_palette = {COLOR_CYAN, COLOR_MAGENTA,
                                                COLOR_YELLOW, COLOR_WHITE};

short getUserColorPair(const std::string &username) {
  if (!colors_ready || username.empty()) {
    return 0;
  }

  auto it = user_pairs.find(username);
  if (it != user_pairs.end()) {
    return it->second;
  }

  const int max_pair = std::min(COLOR_PAIRS - 1, 255);
  const int available_slots = max_pair - 3;

  if (available_slots <= 0) {
    return 0;
  }

  short pair;

  if (next_pair <= max_pair) {
    pair = next_pair;

    short foreground;
    if (COLORS >= 256) {
      foreground = extended_palette[next_palette % extended_palette.size()];
    } else {
      foreground = basic_palette[next_palette % basic_palette.size()];
    }

    if (init_pair(pair, foreground, background_color) == ERR) {
      return 0;
    }

    ++next_pair;
    ++next_palette;
  } else {
    pair = static_cast<short>(4 + std::hash<std::string>{}(username) %
                                      available_slots);
  }

  user_pairs.emplace(username, pair);
  return pair;
}

} // namespace

void initPrintColors() {
  if (!has_colors() || start_color() == ERR) {
    colors_ready = false;
    return;
  }

  background_color = use_default_colors() == ERR ? COLOR_BLACK : -1;

  const short blue = COLORS >= 256 ? 39 : COLOR_BLUE;
  const short green = COLORS >= 256 ? 46 : COLOR_GREEN;
  const short red = COLORS >= 256 ? 196 : COLOR_RED;

  if (init_pair(1, blue, background_color) == ERR ||
      init_pair(2, green, background_color) == ERR ||
      init_pair(3, red, background_color) == ERR) {
    colors_ready = false;
    return;
  }

  colors_ready = true;
}

void setupNcurses() {
  initscr();
  cbreak();
  echo();

  keypad(stdscr, true);

  initPrintColors();

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

void cleanupNcurses() {
  delwin(chatWin);
  delwin(chatBorder);
  delwin(inputWin);
  delwin(inputBorder);
  chatWin = chatBorder = inputWin = inputBorder = nullptr;
  endwin();
}

void print_message(const std::string &message, PrintKind kind,
                   const std::string &sender) {
  if (!chatWin || !inputWin) {
    return;
  }

  short pair = 0;

  std::string formatted_msg;

  switch (kind) {
  case PrintKind::Self:
    formatted_msg = "[YOU]: " + message;
    pair = 2;
    break;
  case PrintKind::Admin:
    formatted_msg = "[ADMIN]: " + message;
    pair = 1;
    break;
  case PrintKind::Error:
    formatted_msg = "[ERROR]: " + message;
    pair = 3;
    break;
  case PrintKind::Client:
    formatted_msg = "[" + sender + "]: " + message;
    pair = getUserColorPair(sender);
    break;
  case PrintKind::Server:
    formatted_msg = "[SERVER]: " + message;
  case PrintKind::System:
  default:
    pair = 0;
    break;
  }

  int attributes = 0;

  if (pair > 0) {
    attributes |= COLOR_PAIR(pair);
  }

  if (kind == PrintKind::System || kind == PrintKind::Server ||
      kind == PrintKind::Error) {
#ifdef A_ITALIC
    attributes |= A_ITALIC;
#endif
    attributes |= A_BOLD;
  }

  if (attributes != 0) {
    wattron(chatWin, attributes);
  }

  wprintw(chatWin, "%s\n", formatted_msg.c_str());

  if (attributes != 0) {
    wattroff(chatWin, attributes);
  }

  wnoutrefresh(chatWin);
  wnoutrefresh(inputWin);
  doupdate();
}
