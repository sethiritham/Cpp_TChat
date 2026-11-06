#include "client/connection.hpp"

EstablishConnectionStatus
ConnectionManager::establish_connection(SSL_CTX *ctx,
                                        sockaddr_in serverAddress) {

  int fd;

  fd = socket(AF_INET, SOCK_STREAM, 0);

  if (connect(fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) <
      0) {
    return EstablishConnectionStatus::ConnectionFailed;
  }

  SSL *ssl = SSL_new(ctx);
  SSL_set_fd(ssl, fd);

  if (SSL_connect(ssl) <= 0) {
    SSL_free(ssl);
    return EstablishConnectionStatus::HandshakeFailed;
  }

  connection.fd = fd;
  connection.ssl = ssl;
  connection.status = ConnectionStatus::AwaitingAuth;

  return EstablishConnectionStatus::Established;
}

void ConnectionManager::close_connection() {

  auto &connection = this->connection;

  SSL *ssl = connection.ssl;
  int fd = connection.fd;

  connection.ssl = nullptr;
  connection.fd = -1;

  connection.status = ConnectionStatus::Closing;

  connection.tx_queue.clear();
  connection.rx_buffer.clear();
  connection.tx_offset = 0;

  connection.read_waiting_for_write = false;
  connection.write_waiting_for_read = false;

  if (ssl)
    SSL_free(ssl);

  if (fd >= 0)
    close(fd);
}
