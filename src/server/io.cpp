#include "server_io.hpp"
#include "server_connection.hpp"
#include <deque>
#include <vector>

void queue_write(int fd, const std::vector<uint8_t> &data,
                 std::deque<std::vector<uint8_t>> &tx_queue) {
  tx_queue.push_back(data);
}

FlushResult flush_write(std::deque<std::vector<uint8_t>> &tx_queue,
                        int &tx_offset, SSL *ssl) {

  while (!tx_queue.empty()) {
    auto &packet = tx_queue.front();

    if (packet.empty()) {
      tx_queue.pop_front();
      tx_offset = 0;
      continue;
    }

    size_t remaining = packet.size() - tx_offset;

    int result = SSL_write(ssl, packet.data(), packet.size());

    if (result > 0) {
      tx_offset += result;

      if (tx_offset >= packet.size()) {
        tx_queue.pop_front();
        tx_offset = 0;
      }

      continue;
    }

    int error = SSL_get_error(ssl, result);

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

    queue_write(fd, packet, connection.tx_queue);
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
