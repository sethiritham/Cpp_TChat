#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/socket.h>

bool set_nonblocking(int fd);

bool set_blocking(int fd);
