#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <ostream>
#include <random>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

enum class SessionState { Connected, Reconnected, Kicked, Expired };

struct ClientConnection {
  int fd;
  SSL *ssl;

  std::vector<uint8_t> rx_buffer;
  std::deque<std::vector<uint8_t>> tx_queue;

  int tx_offset = 0;

  time_t last_seen = time(nullptr);

  ClientConnection(int fd, SSL *ssl) : fd(fd), ssl(ssl) {
    rx_buffer.reserve(65536);
  }
};

struct ClientSession {
  std::string username;
  std::string token;

  SessionState state = SessionState::Connected;
};

class ClientManager {
private:
  std::map<int, ClientConnection> connections_;
  std::map<std::string, ClientSession> sessions_;

public:
  bool kick(const std::string &username);

  ClientSession *find_session(const std::string &username);
  ClientConnection *find_connection(const int &fd);

  void for_each_connection(
      const std::function<void(int, ClientConnection &)> &callback);
};
