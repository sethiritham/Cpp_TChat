#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <ncurses.h>
#include <netdb.h>
#include <netinet/in.h>
#include <random>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

inline std::mutex consoleLock;

inline std::map<std::string, int> clientSockets;
inline std::mutex clientMutex;

inline WINDOW *chatBorder, *chatWin;
inline WINDOW *inputBorder, *inputWin;
inline int screenHeight, screenWidth;

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

inline void print_message(WINDOW *win, const std::string &msg) {}
