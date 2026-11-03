#include "client/io.hpp"
#include "client/connection.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include <sstream>

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

  if (!connection || !connection->ssl)
    return ReadResult::FatalError;

  if (connection->status == ConnectionStatus::Closing)
    return ReadResult::Ignored;

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

    if (error == SSL_ERROR_WANT_READ) {
      connection->read_waiting_for_write = false;
      return ReadResult::WantRead;
    }

    if (error == SSL_ERROR_WANT_WRITE) {
      connection->read_waiting_for_write = true;
      return ReadResult::WantWrite;
    }

    if (error == SSL_ERROR_ZERO_RETURN)
      return ReadResult::ConnectionClosed;

    if (error == SSL_ERROR_SYSCALL && saved_errno == EINTR)
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
    return {};

  auto last = ip.find_last_not_of(" \t\r\n");

  std::string data = ip.substr(first, last - first + 1);

  if (data.empty())
    data = "127.0.0.1";

  return data;
}
