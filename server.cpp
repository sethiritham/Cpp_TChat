#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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
    std::cout << "Server listening on port 12345" << std::endl;

    int clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == -1)
    {
        std::cout << "Error accepting connection" << std::endl;
        return -1;
    }

    std::cout << "Client connected" << std::endl;

    const char *message = "Hello from server";
    send(clientSocket, message, strlen(message), 0);

    
    close(clientSocket);
    close(serverSocket);
    return 0;
}