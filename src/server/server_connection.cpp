#include "server_connection.hpp"
#include <string>
#include <vector>

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
