#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include <thread>
#include "chat.h"


int main()
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        std::cout << "Error creating socket" << std::endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        std::cout << "Error connecting to server" << std::endl;
        return -1;
    }

    std::cout << "Connected! Verifying credentials..." << std::endl;

    std::string name;
    std::string pass;

    std::cout << "Enter your name: " << std::flush;
    std::getline(std::cin, name);

    std::cout << "Enter your password: " << std::flush;
    std::getline(std::cin, pass);

    std::string credentials = name + "|" + pass;
    send(clientSocket, credentials.c_str(), credentials.size(), 0);

    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::string response = buffer;

    if(response == "OK")
    {
        std::cout << "Login successful\n" << name << " is now connected to the server" << std::endl;
    }
    else
    {
        std::cout << "Error connecting to server (WRONG PASSWORD)" << std::endl;
        return -1;
    }

    std::thread t(recieveLoop, clientSocket);
    t.detach();

    sendLoop(clientSocket, name);

    close(clientSocket);
    return 0;
}