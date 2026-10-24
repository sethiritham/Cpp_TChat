#include "server_connection.hpp"
#include "server_io.hpp"
#include "ui.hpp"
#include <deque>
#include <sstream>
#include <string>
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
