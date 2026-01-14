#ifndef CHAT_H
#define CHAT_H

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <mutex>
#include <thread>
#include <arpa/inet.h>

inline std::mutex consoleLock;

inline void safePrint(const char* msg)
{
    std::lock_guard<std::mutex> lock(consoleLock);
    std::cout << "\r\033[K";
    std::cout << msg << std::endl;
    std::cout << "You: " << std::flush;
}

inline void sendLoop(int clientSocket)
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


        send(clientSocket, text.c_str(), text.size(), 0);

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
            break;
        }

        buffer[bytesReceived] = '\0';

        
        safePrint(buffer);
    }
}
#endif