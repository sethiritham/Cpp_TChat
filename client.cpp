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

    std::cout << "Connected to server" << std::endl;

    std::thread t(recieveLoop, clientSocket);
    t.detach();

    sendLoop(clientSocket);

    
    close(clientSocket);
    return 0;
}