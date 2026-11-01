#pragma once

#include <map>
#include <mutex>
#include <ncurses.h>
#include <string>

// Shared state (defined in src/common/ui.cpp)
extern std::mutex consoleLock;
extern std::map<std::string, int> clientSockets;
extern std::mutex clientMutex;
extern WINDOW *chatBorder, *chatWin;
extern WINDOW *inputBorder, *inputWin;
extern int screenHeight, screenWidth;

enum class PrintKind { Self, Server, System, Client, Admin, Error };

void setupNcurses();
void cleanupNcurses();
void initPrintColors();
void print_message(const std::string &message, PrintKind kind,
                   const std::string &sender = "");
