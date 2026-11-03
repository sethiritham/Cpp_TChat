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

enum class EstablishConnectionStatus {
  ConnectionFailed,
  HandshakeFailed,
  Established
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

  Clock::time_point last_server_activity = Clock::now();

  ConnectionStatus status = ConnectionStatus::AwaitingAuth;

  ClientConnection(int fd, SSL *ssl) : fd(fd), ssl(ssl) {}
};

class ConnectionManager {
public:
  ClientConnection *connection = nullptr;

public:
  ConnectionManager() = default;

  EstablishConnectionStatus establish_connection(SSL_CTX *ctx,
                                                 sockaddr_in serverAddress);

  void close_connection();

  ~ConnectionManager() { delete connection; }
};
