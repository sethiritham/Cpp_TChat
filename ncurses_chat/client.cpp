#include "chat.h"

void RunClient()
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        std::cerr<<"Error connecting to server"<<std::endl;
        return;
    }

    std::string name, pass;
    std::cout<<"Enter your name: ";
    std::getline(std::cin, name);
    std::cout<<"Enter your password: ";
    std::getline(std::cin, pass);
    std::string creds = name + "|" + pass;
    send(clientSocket, creds.c_str(), creds.size(), 0);
    
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if(std::string(buffer) != "OK")
    {
        std::cerr<<"Invalid credentials"<<std::endl;
        close(clientSocket);
        return;
    }

    setupNcurses();
    safePrint("Connected to server!\n WELCOME TO TCHAT\n");

    std::thread t(recieveLoop, clientSocket);
    t.detach();

    sendLoop(clientSocket, name);

    cleanupNcurses();
    close(clientSocket);
}
