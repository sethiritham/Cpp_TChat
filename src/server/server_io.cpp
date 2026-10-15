#include "server_io.hpp"

void queue_write(int fd, const std::vector<uint8_t> &data,
                 ClientManager &manager) {
  ClientConnection *connection = manager.find_connection(fd);

  connection->tx_queue.push_back(data);
}

FlushResult flush_write(ClientConnection *connection) {
  if (connection->fd < 0 || connection->ssl == nullptr) {
    return FlushResult::FatalError;
  }

  while (!connection->tx_queue.empty()) {
    auto &packet = connection->tx_queue.front();

    if (packet.empty()) {
      connection->tx_queue.pop_front();
      connection->tx_offset = 0;
      continue;
    }

    size_t remaining = packet.size() - connection->tx_offset;

    int result = SSL_write(connection->ssl, packet.data(), packet.size());

    connection->rx_buffer.insert(connection->rx_buffer.end(), packet.begin(),
                                 packet.end());

    if (result > 0) {
      connection->tx_offset += result;

      if (connection->tx_offset >= packet.size()) {
        connection->tx_queue.pop_front();
        connection->tx_offset = 0;
      }

      continue;
    }

    int error = SSL_get_error(connection->ssl, result);

    if (error == SSL_ERROR_WANT_READ) {
      return FlushResult::ReadWait;
    }

    if (error == SSL_ERROR_WANT_WRITE) {
      return FlushResult::WriteWait;
    }

    return FlushResult::FatalError;
  }

  return FlushResult::QueueEmpty;
}

void broadcast(int sender_fd, const std::vector<uint8_t> &packet,
               ClientManager &manager) {
  manager.for_each_connection([&](int fd, ClientConnection &connection) {
    if (fd == sender_fd) {
      return;
    }

    queue_write(fd, packet, manager);
  });
}

ReadResult handle_read(ClientConnection *connection) {

  SSL *ssl = connection->ssl;
  char buffer[1024];
  ssize_t bytes_read = SSL_read(ssl, buffer, sizeof(buffer));

  if (bytes_read > 0) {
    connection->rx_buffer.insert(connection->rx_buffer.end(), buffer,
                                 buffer + bytes_read);
  } else {
    int ssl_err = SSL_get_error(ssl, bytes_read);
    if (ssl_err == SSL_ERROR_WANT_READ) {
      return ReadResult::WantRead;
    }
    if (ssl_err == SSL_ERROR_WANT_WRITE) {
      return ReadResult::WantWrite;
    }

    return ReadResult::FatalError;
  }

  return ReadResult::DataReceived;
}
