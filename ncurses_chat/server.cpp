#include "chat.h"

void serverSendLoop()
{
    char buffer[1024];

    while(true)
    {
        werase(inputWin);
        wrefresh(inputWin);

        mvwprintw(inputBorder, 0, 1, "[ MESSAGE ]");
        wrefresh(inputBorder);

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


        if(text.rfind("/kick ", 0) == 0)
        {
            std::string targetName = text.substr(6);
            std::lock_guard<std::mutex> lock(clientMutex);
            if (clientSockets.count(targetName) == 0)
            {
                safePrint("User not found");
                continue;
            }
            int targetSocket = clientSockets[targetName];
            std::string kickCmd = "||KICK||";
            send(targetSocket, kickCmd.c_str(), kickCmd.size(), 0);

            safePrint("[SERVER]: " + targetName + " has been kicked");
            broadcast("[SERVER]: " + targetName + " has been kicked", -1);

            continue;
        }

        if(text.empty()) continue;

        safePrint("You: " + text);

        broadcast("[ADMIN]: " + text, -1);
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
        clientSockets[name] = clientSocket;
    }

    while(true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {

            bool wasKicked = false;
            {
                std::lock_guard<std::mutex> lock(clientMutex);
                if (clientSockets.find(name) == clientSockets.end()) wasKicked = true;
            }

            if (wasKicked) break;

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
        if (clientSockets.count(name)) {
            clientSockets.erase(name);
        }
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

    setupNcurses();
    safePrint("Server started on port 12345");
    safePrint("Password: " + SERVER_PASSWORD);
    safePrint("Waiting for clients to join");

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

    serverSendLoop();
    
    cleanupNcurses();
    close(serverSocket);
}
