#include "server/io.hpp"
#include "common/ui.hpp"
#include "server/manager.hpp"
#include <deque>
#include <sstream>
#include <string>
#include <vector>

void queue_write(ClientConnection *connection,
                 const std::vector<uint8_t> &data) {
  connection->tx_queue.push_back(data);
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

void broadcast(int sender_fd, const std::vector<uint8_t> &packet,
               ClientManager &manager) {
  manager.for_each_connection([&](int fd, ClientConnection &connection) {
    if (fd == sender_fd) {
      return;
    }

    queue_write(&connection, packet);
  });
}

ReadResult handle_read(ClientConnection *connection) {

  if (!connection || !connection->ssl)
    return ReadResult::FatalError;

  if (connection->status == ConnectionStatus::Closing)
    return ReadResult::Ignored;

  char buffer[1024];

  while (true) {
    ERR_clear_error();
    errno = 0;

    int bytes_read = SSL_read(connection->ssl, buffer, sizeof(buffer));

    int saved_errno = errno;

    if (bytes_read > 0) {
      connection->rx_buffer.insert(connection->rx_buffer.end(), buffer,
                                   buffer + bytes_read);

      connection->read_waiting_for_write = false;

      return ReadResult::DataReceived;
    }

    int ssl_err = SSL_get_error(connection->ssl, bytes_read);

    if (ssl_err == SSL_ERROR_WANT_READ) {
      connection->read_waiting_for_write = false;
      return ReadResult::WantRead;
    }

    if (ssl_err == SSL_ERROR_WANT_WRITE) {
      connection->read_waiting_for_write = true;
      return ReadResult::WantWrite;
    }

    if (ssl_err == SSL_ERROR_ZERO_RETURN)
      return ReadResult::ConnectionClosed;

    if (ssl_err == SSL_ERROR_SYSCALL && saved_errno == EINTR)
      continue;

    return ReadResult::FatalError;
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

      if (command == "/kick") {
        std::string username;
        std::string extra;

        if (!(input >> username) || (input >> extra)) {
          return {ConsoleInputStatus::InvalidCommand,
                  "Usage: /kick <username>"};
        }

        return {ConsoleInputStatus::KickClient, username};
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
