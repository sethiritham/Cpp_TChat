#include "server/connection.hpp"
#include "common/network.hpp"
#include "common/protocol.hpp"
#include "server/io.hpp"
#include <chrono>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

void ClientManager::enable_write_event(int kq, int fd) {
  struct kevent ev;

  EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);

  if (kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
    perror("Failed to enable write event");
  }
}

void ClientManager::disable_write_event(int kq, int fd) {
  struct kevent ev;

  EV_SET(&ev, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, nullptr);

  if (kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
    perror("Failed to disable write event");
  }
}

AcceptClientStatus ClientManager::accept_client(int server_fd, SSL_CTX *ssl_ctx,
                                                int kq) {
  if (ssl_ctx == nullptr) {
    return AcceptClientStatus::FailedHandshake;
  }

  sockaddr_in client_addr{};
  socklen_t len = sizeof(client_addr);

  int client_fd =
      accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &len);

  if (client_fd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return AcceptClientStatus::NoPendingConnection;
    }

    perror("accept");
    return AcceptClientStatus::FailedHandshake;
  }

  set_blocking(client_fd);

  SSL *ssl = SSL_new(ssl_ctx);

  if (ssl == nullptr) {
    close(client_fd);
    return AcceptClientStatus::FailedHandshake;
  }

  if (SSL_set_fd(ssl, client_fd) != 1) {
    SSL_free(ssl);
    close(client_fd);
    return AcceptClientStatus::FailedHandshake;
  }

  if (SSL_accept(ssl) != 1) {
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    close(client_fd);
    return AcceptClientStatus::FailedHandshake;
  }

  set_nonblocking(client_fd);

  auto [it, inserted] = connections_.try_emplace(client_fd, client_fd, ssl);

  if (!inserted) {
    SSL_free(ssl);
    close(client_fd);
    return AcceptClientStatus::FailedHandshake;
  }

  struct kevent ev{};
  EV_SET(&ev, client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);

  if (kevent(kq, &ev, 1, nullptr, 0, nullptr) == -1) {
    it->second.ssl = nullptr;
    SSL_free(ssl);
    close(client_fd);
    connections_.erase(it);
    return AcceptClientStatus::FailedHandshake;
  }

  return AcceptClientStatus::AcceptedClient;
}

DisconnectStatus ClientManager::disconnect(ClientConnection *connection) {
  if (!connection) {
    return DisconnectStatus::ClientNotFound;
  }

  auto it = connections_.find(connection->fd);

  if (it == connections_.end() || &it->second != connection) {
    return DisconnectStatus::ClientNotFound;
  }

  for (auto &[username, session] : sessions_) {
    if (session.connection == connection) {
      session.connection = nullptr;

      if (session.state == SessionState::Connected) {
        session.state = SessionState::Disconnected;
        session.disconnected_at = ClientSession::Clock::now();
      }

      break;
    }
  }

  int fd = it->first;
  SSL *ssl = it->second.ssl;

  it->second.ssl = nullptr;

  if (ssl) {
    SSL_free(ssl);
  }

  close(fd);

  connections_.erase(it);

  return DisconnectStatus::Disconnected;
}

RemoveSessionStatus ClientManager::remove_session(const std::string &username) {

  ClientSession *session = find_session(username);

  if (!session)
    return RemoveSessionStatus::SessionNotFound;

  if (session->connection != nullptr) {
    return RemoveSessionStatus::SessionStillConnected;
  }

  switch (session->state) {
  case SessionState::Disconnected:
  case SessionState::Kicked:
  case SessionState::Expired:
    break;

  case SessionState::Connected:
  case SessionState::Reconnected:
    return RemoveSessionStatus::InvalidState;
  }

  sessions_.erase(session->username);

  return RemoveSessionStatus::Removed;
}

KickStatus ClientManager::kick(const std::string &username) {
  ClientSession *session = find_session(username);

  if (!session) {
    return KickStatus::ClientNotFound;
  }

  session->state = SessionState::Kicked;

  if (session->connection) {
    DisconnectStatus status = disconnect(session->connection);

    if (status != DisconnectStatus::Disconnected) {
      return KickStatus::ClientNotFound;
    }
  }

  return KickStatus::Kicked;
}

ClientSession *ClientManager::find_session(const std::string &username) {
  for (auto &[name, session] : sessions_) {
    if (name == username) {
      ClientSession *ses = &session;
      return ses;
    }
  }

  return nullptr;
}

ClientConnection *ClientManager::find_connection(const int &fd) {
  for (auto &[connection_fd, connection] : connections_) {
    if (connection_fd == fd) {
      ClientConnection *con = &connection;
      return con;
    }
  }

  return nullptr;
}

void ClientManager::pong(ClientConnection *connection) {
  connection->last_seen = ClientConnection::Clock::now();

  std::vector<uint8_t> pong_packet = create_packet_stream(PacketType::Pong, "");
  queue_write(connection->fd, pong_packet, connection->tx_queue);
}

ClientSession *
ClientManager::find_sesssion_from_connection(ClientConnection *connection) {
  for (auto &[username, session] : sessions_) {
    if (session.connection == connection) {
      return &session;
    }
  }

  return nullptr;
}

void ClientManager::reconnect_client(std::string username,
                                     ClientConnection *connection) {

  auto it = connections_.find(connection->fd);

  auto sess_it = sessions_.find(username);

  if (sess_it != sessions_.end() && it != connections_.end()) {
    if (sessions_[username].state == SessionState::Disconnected) {
      sessions_[username].connection = connection;
      sessions_[username].state = SessionState::Reconnected;
      sessions_[username].disconnected_at = {};

      connection->status = ConnectionStatus::Authenticated;
      connection->last_seen = ClientConnection::Clock::now();
    }
  }
}

void ClientManager::mark_timed_out_connections() {
  using Clock = std::chrono::steady_clock;
  std::vector<int> timed_out_fds;

  for (auto &[fd, connection] : connections_) {
    if (connection.status == ConnectionStatus::Closing)
      continue;

    if (Clock::now() - connection.last_seen >= std::chrono::seconds(10)) {
      connection.status = ConnectionStatus::Closing;
    }
  }
}

void ClientManager::remove_timed_out_connections() {
  for (auto it = connections_.begin(); it != connections_.end();) {

    auto current = it++;

    if (current->second.status == ConnectionStatus::Closing) {
      disconnect(&current->second);
    }
  }
}

void ClientManager::mark_expired_sessions() {
  const auto now = ClientConnection::Clock::now();
  constexpr auto grace_period = std::chrono::seconds(60);

  for (auto &[username, session] : sessions_) {
    if (session.state != SessionState::Disconnected) {
      continue;
    }

    if (session.connection != nullptr) {
      continue;
    }

    if (now - session.disconnected_at >= grace_period) {
      session.state = SessionState::Expired;
    }
  }
}

void ClientManager::remove_expired_sessions() {
  std::vector<std::string> expired_usernames;

  for (const auto &[username, session] : sessions_) {
    if (session.state == SessionState::Expired) {
      expired_usernames.push_back(username);
    }
  }

  for (const auto &username : expired_usernames) {
    RemoveSessionStatus status = remove_session(username);
  }
}

bool ClientManager::verify_session(const std::string &username,
                                   const std::string &token) const {
  auto it = sessions_.find(username);

  if (it == sessions_.end()) {
    return false;
  }

  return it->second.token == token;
}

void ClientManager::for_each_connection(
    const std::function<void(int, ClientConnection &)> &callback) {
  for (auto &[fd, connection] : connections_) {
    callback(fd, connection);
  }
}
