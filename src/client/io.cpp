#include "client/io.hpp"
#include "client/connection.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sstream>
#include <sys/socket.h>

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

FlushResult flush_write(ClientConnection *connection) {

  if (!connection || !connection->ssl)
    return FlushResult::FatalError;

  while (!connection->tx_queue.empty()) {

    auto &packet = connection->tx_queue.front();

    if (packet.empty()) {
      connection->tx_queue.pop_front();
      connection->tx_offset = 0;
      continue;
    }

    if (connection->tx_offset > packet.size())
      return FlushResult::FatalError;

    size_t remaining = packet.size() - connection->tx_offset;

    if (remaining > static_cast<size_t>(INT_MAX))
      return FlushResult::FatalError;

    ERR_clear_error();
    errno = 0;

    int result =
        SSL_write(connection->ssl, packet.data() + connection->tx_offset,
                  static_cast<int>(remaining));

    int saved_errno = errno;

    if (result > 0) {

      connection->tx_offset += static_cast<size_t>(result);
      connection->write_waiting_for_read = false;

      if (connection->tx_offset == packet.size()) {
        connection->tx_queue.pop_front();
        connection->tx_offset = 0;
      }

      continue;
    }

    int error = SSL_get_error(connection->ssl, result);

    if (error == SSL_ERROR_WANT_READ) {
      connection->write_waiting_for_read = true;
      return FlushResult::ReadWait;
    }

    if (error == SSL_ERROR_WANT_WRITE) {
      connection->write_waiting_for_read = false;
      return FlushResult::WriteWait;
    }

    if (error == SSL_ERROR_SYSCALL && saved_errno == EINTR)
      continue;

    return FlushResult::FatalError;
  }

  connection->write_waiting_for_read = false;

  return FlushResult::QueueEmpty;
}

ReadResult handle_read(ClientConnection *connection) {

  if (!connection) {
    return ReadResult::FatalError;
  }

  if (connection->fd < 0) {
    return ReadResult::FatalError;
  }

  if (!connection->ssl) {
    return ReadResult::FatalError;
  }

  // 2. Inspect socket configuration
  int flags = fcntl(connection->fd, F_GETFL, 0);

  if (flags == -1) {
    std::cerr << "[READ] fcntl failed: " << strerror(errno) << '\n';
  } else {
  }

  // 3. Check connection state
  if (connection->status == ConnectionStatus::Closing) {
    return ReadResult::Ignored;
  }

  std::array<uint8_t, MAX_MESSAGE_PAYLOAD_SIZE> buffer{};

  while (true) {

    ERR_clear_error();
    errno = 0;

    int bytes_read = SSL_read(connection->ssl, buffer.data(),
                              static_cast<int>(buffer.size()));

    int saved_errno = errno;

    if (bytes_read > 0) {

      connection->rx_buffer.insert(connection->rx_buffer.end(), buffer.begin(),
                                   buffer.begin() + bytes_read);

      connection->read_waiting_for_write = false;

      return ReadResult::DataReceived;
    }

    int error = SSL_get_error(connection->ssl, bytes_read);

    auto print_openssl_errors = []() {
      unsigned long code;
      bool found = false;

      while ((code = ERR_get_error()) != 0) {

        found = true;

        char error_buffer[256]{};

        ERR_error_string_n(code, error_buffer, sizeof(error_buffer));
      }

      if (!found) {
        std::cerr << "[READ] OpenSSL error queue is empty\n";
      }
    };

    // 7. Classify the error
    switch (error) {

    case SSL_ERROR_NONE:

      return ReadResult::FatalError;

    case SSL_ERROR_WANT_READ:

      connection->read_waiting_for_write = false;

      return ReadResult::WantRead;

    case SSL_ERROR_WANT_WRITE:

      connection->read_waiting_for_write = true;

      return ReadResult::WantWrite;

    case SSL_ERROR_ZERO_RETURN:

      std::cerr << "[READ] SSL_ERROR_ZERO_RETURN\n";
      std::cerr << "[READ] Peer sent TLS close_notify\n";

      std::cerr << "[READ] SSL shutdown flags: "
                << SSL_get_shutdown(connection->ssl) << '\n';

      return ReadResult::ConnectionClosed;

    case SSL_ERROR_SYSCALL: {

      std::cerr << "[READ] SSL_ERROR_SYSCALL\n";

      int socket_error = 0;
      socklen_t socket_error_length = sizeof(socket_error);

      if (getsockopt(connection->fd, SOL_SOCKET, SO_ERROR, &socket_error,
                     &socket_error_length) == 0) {

        std::cerr << "[READ] SO_ERROR: " << socket_error << " ("
                  << strerror(socket_error) << ")\n";

      } else {
        std::cerr << "[READ] getsockopt failed: " << strerror(errno) << '\n';
      }

      print_openssl_errors();

      if (saved_errno == EINTR) {
        std::cerr << "[READ] Interrupted system call. Retrying...\n";
        continue;
      }

      if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {

        std::cerr << "[READ] Socket would block\n";
        return ReadResult::WantRead;
      }

      if (saved_errno == 0) {
        std::cerr << "[READ] Unexpected EOF or connection "
                     "closed without proper TLS shutdown\n";
      }

      return ReadResult::FatalError;
    }

    case SSL_ERROR_SSL:

      std::cerr << "[READ] SSL_ERROR_SSL\n";
      std::cerr << "[READ] TLS protocol/internal SSL failure\n";

      print_openssl_errors();

      return ReadResult::FatalError;

    default:

      std::cerr << "[READ] UNKNOWN SSL ERROR: " << error << '\n';

      print_openssl_errors();

      return ReadResult::FatalError;
    }
  }
}

ConsoleInputResult read_console_input(std::string &input_buffer) {
  int ch;

  while ((ch = wgetch(inputWin)) != ERR) {

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      std::string line = input_buffer;
      input_buffer.clear();

      werase(inputWin);
      wrefresh(inputWin);

      if (line.empty()) {
        continue;
      }

      std::istringstream input(line);
      std::string command;
      input >> command;

      if (command == "/quit") {
        std::string extra;

        if (input >> extra) {
          return {ConsoleInputStatus::InvalidCommand, "Usage: /quit"};
        }

        return {ConsoleInputStatus::Quit, ""};
      }

      if (command.front() == '/') {
        return {ConsoleInputStatus::InvalidCommand,
                "Unknown command: " + command};
      }

      return {ConsoleInputStatus::Message, line};
    }

    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (!input_buffer.empty()) {
        input_buffer.pop_back();
      }
    } else if (ch >= 0 && ch <= UCHAR_MAX &&
               std::isprint(static_cast<unsigned char>(ch))) {
      if (input_buffer.size() < 1024) {
        input_buffer.push_back(static_cast<char>(ch));
      }
    }

    werase(inputWin);
    mvwaddnstr(inputWin, 0, 0, input_buffer.c_str(),
               static_cast<int>(input_buffer.size()));
    wrefresh(inputWin);
  }

  return {ConsoleInputStatus::NoInput, ""};
}

std::string read_server_ip() {

  std::array<char, 64> buffer{};

  printw("Enter server IP (Enter for localhost 127.0.0.1): ");
  refresh();

  echo();

  int result = getnstr(buffer.data(), static_cast<int>(buffer.size() - 1));

  noecho();

  if (result == ERR)
    return {};

  std::string ip(buffer.data());

  auto first = ip.find_first_not_of(" \t\r\n");

  if (first == std::string::npos)
    return "127.0.0.1";

  auto last = ip.find_last_not_of(" \t\r\n");

  return ip.substr(first, last - first + 1);
}
