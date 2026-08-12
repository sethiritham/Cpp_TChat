#include "chat.h"

std::vector<int> clientSockets;
std::mutex clientMutex;

void broadcast(const std::string& message, int senderSocket)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for (int client : clientSockets)
    {
        if (client != senderSocket)
        {
            send(client, message.c_str(), message.size(), 0);
        }
    }
}

void serverSendLoop(int clientSocket)
{
    std::string text;
    while(true)
    {
        std::getline(std::cin, text);
        if(text == "/quit")
        {
            std::cout << "Shutting down server..." << std::endl;
            exit(0);
        }


        std::cout << "\033[A\033[K" << std::flush;
        safePrint("You: " + text);
        broadcast("[ADMIN]: " + text, -1);
    }
}

void HandleClient(int clientSocket)
{
    char buffer[1024] = {0};
    int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytes <= 0)
    {
        close(clientSocket);
        return;
    }

    buffer[bytes] = '\0';
    std::string creds = buffer;
    size_t delimiter = creds.find('|');
    if (delimiter == std::string::npos)
    {
        close(clientSocket);
        return;
    }
    std::string name = creds.substr(0, delimiter);
    std::string password = creds.substr(delimiter + 1);
    if (password != SERVER_PASSWORD)
    {
        send(clientSocket, "NO", 2, 0);
        close(clientSocket);
        return;
    }
    send(clientSocket, "OK", 2, 0);

    std::string joinMsg = "[SERVER]: " + name + " has joined the chat!";
    safePrint(joinMsg);
    broadcast(joinMsg, clientSocket);
    
    {
    std::lock_guard<std::mutex> lock(clientMutex);
    clientSockets.push_back(clientSocket);
    }

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            std::string leaveMsg = "[SERVER]: " + name + " has left the chat!";
            safePrint(leaveMsg);
            broadcast(leaveMsg, clientSocket);
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
    if (serverSocket == -1)
    {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        std::cerr<<"Port "<<12345<<" is already in use!"<<std::endl;
        return;
    }
    listen(serverSocket, 5);
    std::cout<<"Room created! Password: "<<SERVER_PASSWORD<<"\nWaiting for clients on port "<<12345<<std::endl;

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

    serverSendLoop(serverSocket);
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
    
    std::cout<<"Connected to server!"<<std::endl;
    std::thread recieveThread(recieveLoop, clientSocket);
    
    recieveThread.detach();

    sendLoop(clientSocket, name);
    close(clientSocket);
}


int main()
{
    setupTerminal();
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
    