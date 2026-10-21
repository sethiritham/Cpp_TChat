#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <queue>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

enum class SessionState {
  Connected,
  Reconnected,
  Kicked,
  Expired,
  Disconnected
};

enum class WriteWait { Read, Write, None };

enum class ConnectionStatus { AwaitingAuth, Authenticated, Closing };

enum class DisconnectStatus { Disconnected, ClientNotFound };

enum class KickStatus { Kicked, ClientNotFound };

enum class AcceptClientStatus {
  FailedHandshake,
  AcceptedClient,
  NoPendingConnection
};

struct ClientConnection {
  int fd = -1;
  SSL *ssl = nullptr;

  std::deque<std::vector<uint8_t>> tx_queue;
  std::vector<uint8_t> rx_buffer;

  int tx_offset = 0;

  time_t last_seen = time(nullptr);

  ConnectionStatus status = ConnectionStatus::AwaitingAuth;

  ClientConnection(int fd, SSL *ssl) : fd(fd), ssl(ssl) {}
};

struct ClientSession {
  std::string username;
  std::string token;

  ClientConnection *connection;

  time_t disconnected_at = time(nullptr);

  SessionState state = SessionState::Connected;
};

class ClientManager {
private:
  std::map<int, ClientConnection> connections_;
  std::map<std::string, ClientSession> sessions_;

private:
  void enable_write_event(int kq, int fd);

  void disable_write_event(int kq, int fd);

  bool authorize_client(const std::vector<uint8_t> &auth_packet);

public:
  KickStatus kick(const std::string &username);
  DisconnectStatus disconnect(ClientConnection *connection);

  ClientSession *find_session(const std::string &username);
  ClientConnection *find_connection(const int &fd);

  ClientSession *find_sesssion_from_connection(ClientConnection *connection);

  void pong(ClientConnection *connection);

  AcceptClientStatus accept_client(int server_fd, SSL_CTX *ssl_ctx, int kq);

  void check_client_timeout();

  void for_each_connection(
      const std::function<void(int, ClientConnection &)> &callback);
};
