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

AcceptClientStatus ClientManager::accept_client(int fd, SSL_CTX *ssl_ctx) {
  ClientConnection *connection = find_connection(fd);
  set_blocking(fd);

  SSL *ssl = SSL_new(ssl_ctx);
  SSL_set_fd(ssl, fd);

  if (SSL_accept(ssl) <= 0) {
    return AcceptClientStatus::FailedHandshake;
  }

  connections_[fd] = ClientConnection(fd, ssl);

  HandshakeResult result = process_handshake_packet(connections_[fd].rx_buffer);

  // manage invalid statuses
  switch (result.status) {
  case HandshakeStreamStatus::InvalidProtocolError: {
    auto error_packet = create_packet_stream(PacketType::Error,
                                             "[SERVER]: Invalid Protocol key");
    queue_write(fd, error_packet, connection->tx_queue);
    return AcceptClientStatus::FailedToAuthorize;
  }
  case HandshakeStreamStatus::IncompletePacket: {
    auto error_packet = create_packet_stream(
        PacketType::Error, "[SERVER] : Incomplete auth packet");
    queue_write(fd, error_packet, connection->tx_queue);

    return AcceptClientStatus::FailedToAuthorize;
  }
  case HandshakeStreamStatus::InvalidAuthPacket: {
    auto error_packet = create_packet_stream(PacketType::Error,
                                             "[SERVER] : Invalid auth packet");
    return AcceptClientStatus::FailedToAuthorize;
  }

  default:
    break;
  }

  // manage Login Register Reconnection
  switch (result.request.kind) {
  case HandshakeKind::Register: {
    register_client(result.request.username, result.request.password);
  }
  case HandshakeKind::Login: {
    verify_user(result.request.username, result.request.password);
  }
  case HandshakeKind::Reconnect: {
    verify_user(result.request.username, result.request.token);
  }

  default:
    break;
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

void ClientManager::handle_client_connection(int server_fd, SSL_CTX *ssl_ctx,
                                             int kq) {
  sockaddr_in client_addr;
  socklen_t len = sizeof(client_addr);

  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);

  if (client_fd >= 0) {
    // client login
  } else {
    // Error
  }
}
