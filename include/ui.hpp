#include <arpa/inet.h>
#include <cstring>
#include <map>
#include <mutex>
#include <ncurses.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

inline std::mutex consoleLock;
inline std::map<std::string, int> clientSockets;
inline std::mutex clientMutex;
inline WINDOW *chatBorder, *chatWin;
inline WINDOW *inputBorder, *inputWin;
inline int screenHeight, screenWidth;

enum class PrintKind { Self, Server, System, Client, Admin, Error };

inline void setupNcurses() {
  initscr();
  cbreak();
  echo();

  keypad(stdscr, true);

  if (has_colors()) {
    start_color();

    use_default_colors();

    init_pair(1, COLOR_CYAN, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_RED, -1);
    init_pair(4, COLOR_YELLOW, -1);
    init_pair(5, COLOR_BLUE, -1);
    init_pair(6, COLOR_MAGENTA, -1);
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
  delwin(chatBorder);
  delwin(inputWin);
  delwin(inputBorder);
  endwin();
}

namespace {

bool colors_ready = false;
short background_color = COLOR_BLACK;

std::unordered_map<std::string, short> user_pairs;
short next_pair = 4;
size_t next_palette = 0;

constexpr std::array<short, 32> extended_palette = {
    202, 208, 214, 220, 226, 190, 154, 118, 82,  48, 51, 45, 33, 69,  99,  129,
    165, 201, 198, 166, 172, 178, 184, 148, 112, 76, 43, 37, 75, 111, 147, 183};

constexpr std::array<short, 4> basic_palette = {COLOR_CYAN, COLOR_MAGENTA,
                                                COLOR_YELLOW, COLOR_WHITE};

} // namespace

inline void initPrintColors() {
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

namespace {

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

inline void print_message(std::string &message, const PrintKind &kind,
                          const std::string &sender = "") {
  if (!chatWin || !inputWin) {
    return;
  }

  short pair = 0;

  switch (kind) {
  case PrintKind::Self:
    pair = 2;
  case PrintKind::Server:
    pair = 0;
  case PrintKind::Admin:
    pair = 1;
  case PrintKind::Error:
    pair = 3;
  case PrintKind::Client:
    pair = getUserColorPair(sender);
  case PrintKind::System:
    pair = 0;

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
    attributes |= A_ITALIC | A_BOLD;
  }

  wprintw(chatWin, "%s\n", message.c_str());

  if (attributes != 0) {
    wattroff(chatWin, attributes);
  }

  wnoutrefresh(chatWin);
  wnoutrefresh(inputWin);
  doupdate();
}
