#include "chat.h"

std::vector<int> clientSockets;
std::mutex clientMutex;

void broadcast(const std::string& message, int senderSocket)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for (int client : clientSockets)
    {
        if (client != senderSocket)
            send(client, message.c_str(), message.size(), 0);
    }
}

void serverSendLoop()
{
    char buffer[1024];

    while(true)
    {
        werase(inputWin);
        box(inputWin, 0, 0);
        mvwprintw(inputWin, 0, 1, "[ ADMIN MESSAGE ]");

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

        broadcast("[ ADMIN ]: " + text, -1);
    }

}

void HandleClient(int clientSocket)
{
    char buffer[1024];
    int bytes = recv(clientSocket, buffer, 1023, 0);

    if(bytes <= 0) { close(clientSocket); return; }

    buffer[bytes] = '\0';
    std::string message = buffer;

    if(message.find("|") == std::string::npos) { close(clientSocket); return; }

    std::string name = message.substr(0, message.find("|"));
    std::string password = message.substr(message.find("|") + 1);

    if(password != SERVER_PASSWORD) { send(clientSocket, "NO", 2, 0);close(clientSocket); return; }

    send(clientSocket, "OK", 2, 0);

    std::string joinMsg = "[SERVER]: " + name + " has joined the chat";
    safePrint(joinMsg);
    broadcast(joinMsg, clientSocket);


    {
        std::lock_guard<std::mutex> lock(clientMutex);
        clientSockets.push_back(clientSocket);
    }

    while(true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            std::string leftMsg = "[SERVER]: " + name + " has left the chat";
            safePrint(leftMsg);
            broadcast(leftMsg, clientSocket);
            break;
        }
        buffer[bytesReceived] = '\0';
        std::string msg = buffer;
        safePrint(msg);
        broadcast(msg, clientSocket);
    }

    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
    }

}


void RunServer()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == -1)
    {
        std::cerr<<"Socket creation failed"<<std::endl;
        return;
    }

    struct sockaddr_in serverAddr;
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    if(bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cout<<"PORT BUSY!"<<std::endl;
        return;
    }
    listen(serverSocket, 5);


    std::thread listener([serverSocket](){
        while (true)
        {
            int clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket < 0)
            {
                std::cerr<<"Error accepting connection"<<std::endl;
                continue;
            }
            std::thread clientThread(HandleClient, clientSocket);
            clientThread.detach();
        }
    });
    listener.detach();

    setupNcurses();
    safePrint("Server started on port 12345");
    safePrint("Password: " + SERVER_PASSWORD);
    safePrint("Waiting for clients to join");

    serverSendLoop();

    cleanupNcurses();
    close(serverSocket);
}

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
    safePrint("Connected to server!\nWELCOME TO TCHAT\n\n");

    std::thread t(recieveLoop, clientSocket);
    t.detach();

    sendLoop(clientSocket, name);

    cleanupNcurses();
    close(clientSocket);
}

int main()
{
    std::cout<<"========== TERMINAL CHAT ==========\n";
    std::cout<<"1. Run Server"<<std::endl;
    std::cout<<"2. Run Client"<<std::endl;
    std::cout<<"Enter your choice: ";
    int choice;
    std::cin>>choice;
    std::cin.ignore();
    if (choice == 1)
    {
        RunServer();
    }
    else if (choice == 2)
    {
        RunClient();
    }
    else
    {
        std::cerr<<"Invalid choice"<<std::endl;
    }
    return 0;
}