#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

struct ClientConnection;

enum class FlushResult { QueueEmpty, WriteWait, ReadWait, FatalError };

enum class ReadResult {
  DataReceived,
  WantRead,
  WantWrite,
  ConnectionClosed,
  Ignored,
  FatalError
};

enum class ConsoleInputStatus {
  NoInput,
  Message,
  KickClient,
  Quit,
  InvalidCommand
};

void queue_write(ClientConnection *connection,
                 const std::vector<uint8_t> &data);

std::string read_username(size_t max_size);

std::string read_password(size_t max_size);

FlushResult flush_write(ClientConnection *connection);

ReadResult handle_read(ClientConnection *connection);
