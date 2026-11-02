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
  ClientConnection connection;

public:
  EstablishConnectionStatus establish_connection(SSL_CTX *ctx,
                                                 sockaddr_in serverAddress) {
    connection.fd = socket(AF_INET, SOCK_STREAM, 0);

    if (connect(connection.fd, (struct sockaddr *)&serverAddress,
                sizeof(serverAddress)) < 0) {
      return EstablishConnectionStatus::ConnectionFailed;
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, connection.fd);

    if (SSL_connect(ssl) <= 0) {
      SSL_free(ssl);
      return EstablishConnectionStatus::HandshakeFailed;
    }

    connection.ssl = ssl;
    connection.status = ConnectionStatus::AwaitingAuth;

    return EstablishConnectionStatus::Established;
  }
};
