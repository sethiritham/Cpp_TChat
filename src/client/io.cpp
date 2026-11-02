#include "client/io.hpp"
#include "client/connection.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"

void queue_write(ClientConnection *connection,
                 const std::vector<uint8_t> &data) {
  connection->tx_queue.push_back(data);
}

std::string read_username(size_t max_size) {
  echo();

  std::string buffer(max_size, '\0');

  getnstr(&buffer[0], max_size - 1);

  buffer.resize(std::strlen(buffer.c_str()));

  noecho();

  return buffer;
}

std::string read_password(size_t max_size) {

  int ch;
  int i = 0;

  char password[64];

  noecho();

  while (i < (int)max_size - 1) {
    ch = getch();

    if (ch == '\n' || ch == '\r') {
      break;
    }

    else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (i > 0) {
        i--;
        printw("\b \b");
      }
    } else if (ch >= 32 && ch <= 126) {
      password[i] = ch;
      i++;
      printw("*");
    }
  }

  password[i] = '\0';

  noecho();

  return std::string(password);
}

SendPacketStatus send_packet(const std::vector<uint8_t> &pkt, SSL *ssl) {
  if (!ssl)
    return SendPacketStatus::InvalidSSL;

  size_t offset = 0;

  while (offset < pkt.size()) {
    size_t remaining = pkt.size() - offset;

    int ssl_result =
        SSL_write(ssl, pkt.data() + offset, static_cast<int>(remaining));

    if (ssl_result > 0) {
      offset += static_cast<size_t>(ssl_result);
      continue;
    }

    int ssl_error = SSL_get_error(ssl, ssl_result);

    if (ssl_error == SSL_ERROR_ZERO_RETURN) {
      return SendPacketStatus::FailedToSend;
    }

    return SendPacketStatus::FailedToSend;
  }

  return SendPacketStatus::PacketSent;
}

ReceiveStatus receive_data(SSL *ssl, std::vector<uint8_t> &rx_buffer) {
  if (!ssl)
    return ReceiveStatus::InvalidSSL;

  std::array<uint8_t, MAX_MESSAGE_PAYLOAD_SIZE> buffer{};

  while (true) {
    ERR_clear_error();
    errno = 0;

    int bytes_read =
        SSL_read(ssl, buffer.data(), static_cast<int>(buffer.size()));

    int saved_errno = errno;

    if (bytes_read > 0) {
      rx_buffer.insert(rx_buffer.end(), buffer.begin(),
                       buffer.begin() + bytes_read);

      return ReceiveStatus::DataReceived;
    }

    int ssl_error = SSL_get_error(ssl, bytes_read);

    switch (ssl_error) {

    case SSL_ERROR_WANT_READ:
      return ReceiveStatus::WantRead;

    case SSL_ERROR_WANT_WRITE:
      return ReceiveStatus::WantWrite;

    case SSL_ERROR_ZERO_RETURN:
      return ReceiveStatus::ConnectionClosed;

    case SSL_ERROR_SYSCALL:
      if (saved_errno == EINTR)
        continue;

      return ReceiveStatus::TransportError;

    case SSL_ERROR_SSL:
      return ReceiveStatus::TLSFailure;

    default:
      return ReceiveStatus::TLSFailure;
    }
  }
}
