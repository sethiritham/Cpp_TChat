#pragma once

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
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
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

enum class RemoveSessionStatus {
  Removed,
  SessionNotFound,
  SessionStillConnected,
  InvalidState
};

struct ClientConnection {
  using Clock = std::chrono::steady_clock;

  int fd = -1;
  SSL *ssl = nullptr;

  std::deque<std::vector<uint8_t>> tx_queue;
  std::vector<uint8_t> rx_buffer;

  int tx_offset = 0;

  Clock::time_point last_seen = Clock::now();

  ConnectionStatus status = ConnectionStatus::AwaitingAuth;

  bool read_waiting_for_write = false;
  bool write_waiting_for_read = false;

  ClientConnection(int fd, SSL *ssl) : fd(fd), ssl(ssl) {}
};

struct ClientSession {

  using Clock = std::chrono::steady_clock;

  std::string username;
  std::string token;

  ClientConnection *connection;

  Clock::time_point disconnected_at = Clock::now();

  SessionState state = SessionState::Connected;
};

class ClientManager {
private:
  std::map<int, ClientConnection> connections_;
  std::map<std::string, ClientSession> sessions_;

private:
  bool authorize_client(const std::vector<uint8_t> &auth_packet);

public:
  KickStatus kick(const std::string &username);
  DisconnectStatus disconnect(ClientConnection *connection);

  ClientSession *find_session(const std::string &username);
  ClientConnection *find_connection(const int &fd);

  ClientSession *find_sesssion_from_connection(ClientConnection *connection);

  RemoveSessionStatus remove_session(const std::string &username);

  void enable_write_event(int kq, int fd);
  void disable_write_event(int kq, int fd);

  void pong(ClientConnection *connection);

  AcceptClientStatus accept_client(int server_fd, SSL_CTX *ssl_ctx, int kq);

  void mark_timed_out_connections();

  void remove_timed_out_connections();

  void mark_expired_sessions();

  void remove_expired_sessions();

  void reconnect_client(std::string username, ClientConnection *connection);

  void check_client_timeout();

  void for_each_connection(
      const std::function<void(int, ClientConnection &)> &callback);
};
