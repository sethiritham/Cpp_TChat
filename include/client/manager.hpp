#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
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

enum class ConnectionStatus { AwaitingAuth, Authenticated, Closing };

enum class SessionState {
  Connected,
  Reconnected,
  Kicked,
  Expired,
  Disconnected
};

struct ClientConnection {
  using Clock = std::chrono::steady_clock;

  int fd = -1;
  SSL *ssl = nullptr;

  std::deque<std::vector<uint8_t>> tx_queue;
  std::vector<uint8_t> rx_buffer;

  int tx_offset = 0;

  bool read_waiting_for_write = false;
  bool write_waiting_for_read = false;

  ConnectionStatus status = ConnectionStatus::AwaitingAuth;

  ClientConnection(int fd, SSL *ssl) : fd(fd), ssl(ssl) {}
};

struct ClientSession {

  using Clock = std::chrono::steady_clock;

  std::string username;
  std::string token;

  ClientConnection *connection;

  SessionState state = SessionState::Connected;
};
