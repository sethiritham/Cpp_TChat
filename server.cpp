#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <mutex>
#include <thread>

std::mutex consoleLock;

void safePrint(const char* msg)
{
    std::lock_guard<std::mutex> lock(consoleLock);
    std::cout << "\r\033[K";
    std::cout << msg << std::endl;
    std::cout << "You: " << std::flush;
}

void sendLoop(int clientSocket)
{
    std::string text;
    while (true)
    {
        std::getline(std::cin, text);
        std::cout << "\033[A\033[K" << std::flush;
        {
            std::lock_guard<std::mutex> lock(consoleLock);
            std::cout << "You: " << text << std::endl;
        }

        send(clientSocket, text.c_str(), text.size(), 0);

        {
            std::lock_guard<std::mutex> lock(consoleLock);
            std::cout << "You: " << text << std::flush;
        }
    }
}

void recieveLoop(int clientSocket)
{
    char buffer[1024] = {0};
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0)
        {
            break;
        }

        buffer[bytesReceived] = '\0';

        
        safePrint(buffer);
    }
}
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

    std::thread t(recieveLoop, clientSocket);
    t.detach();

    sendLoop(clientSocket);

    
    close(clientSocket);
    close(serverSocket);
    return 0;
}