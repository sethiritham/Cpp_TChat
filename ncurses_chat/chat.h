#ifndef CHAT_H
#define CHAT_H

#include <iostream>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <random>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <ncurses.h>

inline std::mutex consoleLock;

inline WINDOW* chatWin;
inline WINDOW* inputWin;
inline int screenHeight, screenWidth;


inline std::string passwordGenerator()
{
    std::random_device rd;
    std::mt19937 gen(rd());                     
    std::uniform_int_distribution<> dist(10000, 99999);

    int randomNumber = dist(gen);

    std::string password = std::to_string(randomNumber);

    return password;
}
const std::string SERVER_PASSWORD = passwordGenerator();

inline void setupNcurses()
{
    initscr();
    cbreak();
    noecho();

    getmaxyx(stdscr, screenHeight, screenWidth);
    chatWin = newwin(screenHeight - 3, screenWidth, 0, 0);
    inputWin = newwin(3, screenWidth, screenHeight - 3, 0);

    scrollok(chatWin, true);
    idlok(chatWin, true);

    box(chatWin, 0, 0);
    box(inputWin, 0, 0);

    wrefresh(chatWin);
    wrefresh(inputWin);
    
}

inline void cleanupNcurses()
{
    delwin(chatWin);
    delwin(inputWin);
    endwin();
}

inline void safePrint(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(consoleLock);
    wprintw(chatWin, "%s\n", msg.c_str());
    
    box(chatWin, 0, 0);
    mvwprintw(chatWin, 0, 1, "[ CHAT ROOM ]");
    
    wrefresh(chatWin);

    wrefresh(inputWin);
}

inline void sendLoop(int clientSocket, std::string name)
{
    char buffer[1024] = {0};
    while(true)
    {
        werase(inputWin);
        box(inputWin, 0, 0);
        mvwprintw(inputWin, 0, 1, "[ MESSAGE ]");

        wmove(inputWin, 1, 2);
        wrefresh(inputWin);

        memset(buffer, 0, sizeof(buffer));
        wgetnstr(inputWin, buffer, 1023);

        std::string text = buffer;

        if(text == "/quit")
        {
            safePrint("Quitting...");
            break;
        }

        if(text.empty()) continue;

        safePrint("You: " + text);

        std::string packet = "[" + name + "]: " + text;
        send(clientSocket, packet.c_str(), packet.size(), 0);

    }
}

inline void recieveLoop(int clientSocket)
{
    char buffer[1024] = {0};
    while(true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            cleanupNcurses();
            std::cout<<"Connection closed"<<std::endl;
            exit(0);
        }
        buffer[bytesReceived] = '\0';
        std::string msg = buffer;
        safePrint(msg);
    }
}

#endif