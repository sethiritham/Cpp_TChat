#include "queue"
#include "server_app.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

struct ClientConnection;
class ClientManager;

enum class FlushResult { QueueEmpty, WriteWait, ReadWait, FatalError };

enum class ReadResult { DataReceived, WantRead, WantWrite, FatalError };

void queue_write(int fd, const std::vector<uint8_t> &data,
                 std::deque<std::vector<uint8_t>> &tx_queue);

FlushResult flush_write(ClientConnection *connection);

void broadcast(int sender_fd, const std::vector<uint8_t> &packet,
               ClientManager &manager);

ReadResult handle_read(ClientConnection *connection);
