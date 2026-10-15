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

enum class WriteWait { Read, Write, None };

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

private:
  void enable_write_event(int kq, int fd) {
    struct kevent ev;

    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);

    if (kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
      perror("Failed to enable write event");
    }
  }

  void disable_write_event(int kq, int fd) {
    struct kevent ev;

    EV_SET(&ev, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, nullptr);

    if (kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
      perror("Failed to disable write event");
    }
  }

public:
  bool kick(const std::string &username);

  ClientSession *find_session(const std::string &username);
  ClientConnection *find_connection(const int &fd);

  void for_each_connection(
      const std::function<void(int, ClientConnection &)> &callback);
};
