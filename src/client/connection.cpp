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

  connection->ssl = ssl;
  connection->status = ConnectionStatus::AwaitingAuth;

  connection = new ClientConnection(fd, ssl);

  return EstablishConnectionStatus::Established;
}

void ConnectionManager::close_connection() {

  if (connection->ssl) {
    SSL_free(connection->ssl);
  }

  close(connection->fd);
}
