#include "chat.h"

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