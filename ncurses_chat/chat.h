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
#include <map>
#include <ncurses.h>

inline std::mutex consoleLock;

inline std::map<std::string, int> clientSockets;
inline std::mutex clientMutex;

inline WINDOW* chatBorder, *chatWin;
inline WINDOW* inputBorder, *inputWin;
inline int screenHeight, screenWidth;

void RunServer();
void RunClient();

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
    echo();

    keypad(stdscr, true);

    if(has_colors())
    {
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

inline void cleanupNcurses()
{
    delwin(chatWin);
    delwin(chatBorder);
    delwin(inputWin);
    delwin(inputBorder);
    endwin();
}

inline void safePrint(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(consoleLock);

    int color_pair = 0;

    if (msg.find("[SERVER]") != std::string::npos || msg.find("[ ADMIN ]") != std::string::npos) 
    {
        color_pair = 1; 
    }
    else if (msg.find("You:") == 0) 
    {
        color_pair = 2; 
    }
    else if (msg.find("Error") != std::string::npos || msg.find("Quitting") != std::string::npos)
    {
        color_pair = 3; 
    }

    if (color_pair > 0) wattron(chatWin, COLOR_PAIR(color_pair));
    
    wprintw(chatWin, "%s\n", msg.c_str());

    if (color_pair > 0) wattroff(chatWin, COLOR_PAIR(color_pair));
    
    wrefresh(chatWin);

    wrefresh(inputWin);
}

inline void sendLoop(int clientSocket, std::string name)
{
    char buffer[1024] = {0};
    while(true)
    {
        werase(inputWin);

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
        if (send(clientSocket, packet.c_str(), packet.size(), 0) == -1) break;

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

        if(msg == "||KICK||")
        {
            cleanupNcurses();
            std::cout<<"You have been kicked by admin"<<std::endl;
            exit(0);
        }
        safePrint(msg);
    }
}

inline void broadcast(const std::string& message, int senderSocket)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for (auto const& [name, socket] : clientSockets)
    {
        if (socket != senderSocket)
            send(socket, message.c_str(), message.size(), 0);
    }
}

#endif