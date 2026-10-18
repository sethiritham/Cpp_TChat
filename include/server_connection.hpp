#pragma once

#include "network.hpp"
#include "protocol.hpp"
#include "server_io.hpp"
#include "server_message_handler.hpp"
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
  AcceptedClient

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

  bool authorize_client(const std::vector<uint8_t> &auth_packet);

  AcceptClientStatus accept_client(int fd, SSL_CTX *ssl_ctx) {
    ClientConnection *connection = find_connection(fd);
    set_blocking(fd);

    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, fd);

    if (SSL_accept(ssl) <= 0) {
      return AcceptClientStatus::FailedHandshake;
    }

    connections_[fd] = ClientConnection(fd, ssl);

    HandshakeResult result =
        process_handshake_packet(connections_[fd].rx_buffer);

    // manage invalid statuses
    switch (result.status) {
    case HandshakeStreamStatus::InvalidProtocolError: {
      auto error_packet = create_packet_stream(
          PacketType::Error, "[SERVER]: Invalid Protocol key");

      queue_write(error_packet, connection->tx_queue);
    }

    default:
      break;
    }

    // manage Login Register Reconnection
    switch (result.request.kind) {
    default:
      break;
    }

    return AcceptClientStatus::AcceptedClient;
  }

public:
  KickStatus kick(const std::string &username);
  DisconnectStatus disconnect(ClientConnection *connection);

  ClientSession *find_session(const std::string &username);
  ClientConnection *find_connection(const int &fd);

  void pong(ClientConnection *connection);

  void handle_client_connection(int server_fd, SSL_CTX *ssl_ctx, int kq);

  void for_each_connection(
      const std::function<void(int, ClientConnection &)> &callback);
};
