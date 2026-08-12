#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include "chat.h"

int main()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        
        std::cout << "Error creating socket" << std::endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        std::cout << "Error binding socket" << std::endl;
        return -1;
    }

    listen(serverSocket, 1);
    std::cout << "Server listening on port 12345, Password is " << SERVER_PASSWORD << std::endl;

    int clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == -1)
    {    
        std::cout << "Error accepting connection" << std::endl;
        return -1;
    }
    std::cout << "Client connected. Waiting for login..." << std::endl;

    char buffer[1024] = {0};
    int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    buffer[bytes] = '\0';

    std::string credentials = buffer;
    std::string name = credentials.substr(0, credentials.find("|"));
    std::string pass = credentials.substr(credentials.find("|") + 1);

    if(pass == SERVER_PASSWORD)
    {
        std::cout << "Access Granted to: " << name << std::endl;
        send(clientSocket, "OK", 2, 0);
    }
    else
    {
        std::cout << "Access Denied to: " << name << std::endl;
        send(clientSocket, "NO", 2, 0);
        close(clientSocket);
        return -1;
    }

    std::thread t(recieveLoop, clientSocket);
    t.detach();

    sendLoop(clientSocket, "ADMIN");
    
    close(clientSocket);
    close(serverSocket);
    return 0;
}