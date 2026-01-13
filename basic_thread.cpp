#include <iostream>
#include <thread>
#include <unistd.h>
#include <mutex>

std::mutex printLock;
void keepPrinting() {
    while (true) {
        std::cout << "Hello from thread" << std::endl;
        sleep(1);
    }
}

void securePrint(const char* msg)
{
    printLock.lock();
    std::cout << msg << std::endl;
    printLock.unlock();
}

int main()
{

    std::thread t1(securePrint, "Hello from thread 1");
    std::thread t2(securePrint, "Hello from thread 2");
    
    while(true)
    {
        std::cout << "Hello from main" << std::endl;
        sleep(2);
    }

    t1.join();
    t2.join();
    return 0;

}