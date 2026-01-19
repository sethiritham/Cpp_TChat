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
#include <termios.h>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

inline std::mutex consoleLock;

inline void setupTerminal()
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_iflag |= ICRNL;
    t.c_oflag |= (ONLCR | OPOST);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

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

inline void safePrint(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(consoleLock);
    std::cout << "\r\033[K";
    std::cout << msg << std::endl;
    std::cout << "You: " << std::flush;
}

inline void sendLoop(int clientSocket, std::string name)
{
    std::string text;
    while (true)
    {
        std::getline(std::cin, text);

        if(text == "/quit")
        {
            std::cout << "Quitting..." << std::endl;
            break;
        }
        std::cout << "\033[A\033[K" << std::flush;
        {
            std::lock_guard<std::mutex> lock(consoleLock);
            std::cout << "You: " << text << std::endl;
        }

        std::string packet = "[" + name + "]: " + text;

        send(clientSocket, packet.c_str(), packet.size(), 0);

        {
            std::lock_guard<std::mutex> lock(consoleLock);
            std::cout << "You: " << std::flush;
        }
    }
}

inline void recieveLoop(int clientSocket)
{
    char buffer[1024] = {0};
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            safePrint("Server disconnected. Press Enter to exit...");
            exit(0);
        }

        buffer[bytesReceived] = '\0';

        
        safePrint(buffer);
    }
}
#endif