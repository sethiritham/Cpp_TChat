#include "server_connection.hpp"
#include "authorization.hpp"
#include "network.hpp"
#include "protocol.hpp"
#include "server_io.hpp"
#include "server_message_handler.hpp"
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
  if (ssl_ctx == nullptr || kq < 0) {
    return AcceptClientStatus::FailedHandshake;
  }

  sockaddr_in client_addr{};
  socklen_t len = sizeof(client_addr);

  int fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &len);

  if (fd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return AcceptClientStatus::NoPendingConnection;
    }

    return AcceptClientStatus::FailedHandshake;
  }

  set_blocking(fd);

  SSL *ssl = SSL_new(ssl_ctx);

  if (ssl == nullptr) {
    close(fd);
    return AcceptClientStatus::FailedHandshake;
  }

  if (SSL_set_fd(ssl, fd) != 1) {
    SSL_free(ssl);
    close(fd);
    return AcceptClientStatus::FailedHandshake;
  }

  if (SSL_accept(ssl) != 1) {
    SSL_free(ssl);
    close(fd);
    return AcceptClientStatus::FailedHandshake;
  }

  auto [it, inserted] = connections_.try_emplace(fd, fd, ssl);

  if (!inserted) {
    SSL_free(ssl);
    close(fd);
    return AcceptClientStatus::FailedHandshake;
  }

  struct kevent ev{};
  EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);

  if (kevent(kq, &ev, 1, nullptr, 0, nullptr) == -1) {
    SSL_free(ssl);
    close(fd);
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
        session.disconnected_at = time(nullptr);
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
  connection->last_seen = time(nullptr);

  std::vector<uint8_t> pong_packet = create_packet_stream(PacketType::Pong, "");
  queue_write(connection->fd, pong_packet, connection->tx_queue);
}
