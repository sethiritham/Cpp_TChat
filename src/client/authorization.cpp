#include "client/authorization.hpp"
#include "client/connection.hpp"
#include "client/io.hpp"
#include "client/message_handler.hpp"
#include "common/network.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <string>

LoginResult login_client(ClientConnection *connection) {

  LoginResult result{};

  if (!connection || !connection->ssl || connection->fd < 0) {
    result.status = LoginStatus::ConnectionLost;
    return result;
  }

  printw("- - - LOGIN - - -\n");

  printw("Username: ");
  refresh();
  std::string username = read_username(36);

  printw("Password: ");
  refresh();
  std::string password = read_password(36);

  std::string payload = username + "|" + password;

  auto login_packet = create_packet_stream(PacketType::Login, payload);

  queue_write(connection, login_packet);

  FlushResult write_status = flush_write(connection);

  if (write_status != FlushResult::QueueEmpty) {
    result.status = LoginStatus::ConnectionLost;
    return result;
  }

  while (true) {

    ReadResult read_status = handle_read(connection);

    if (read_status != ReadResult::DataReceived) {
      result.status = LoginStatus::ConnectionLost;
      return result;
    }

    std::string msg;

    auto process_status = process_data_stream(connection->rx_buffer, msg);

    if (process_status == ProcessStreamResult::IncompletePacket)
      continue;

    if (process_status == ProcessStreamResult::InvalidProtocolError) {
      result.status = LoginStatus::ProtocolError;
      return result;
    }

    if (msg == "NO") {
      result.status = LoginStatus::AuthenticationRejected;
      return result;
    }

    if (msg.rfind("OK|", 0) == 0 && msg.size() > 3) {
      result.username = username;
      result.token = msg.substr(3);
      result.status = LoginStatus::Success;
      return result;
    }

    result.status = LoginStatus::ProtocolError;
    return result;
  }
}

RegisterStatus register_client(ClientConnection *connection) {

  if (!connection) {
    return RegisterStatus::ConnectionLost;
  }

  if (connection->fd < 0) {
    return RegisterStatus::ConnectionLost;
  }

  if (!connection->ssl) {
    return RegisterStatus::ConnectionLost;
  }

  int flags = fcntl(connection->fd, F_GETFL, 0);

  printw("- - - REGISTER - - -\n");

  printw("Username: ");
  refresh();

  std::string username = read_username(36);

  std::cerr << "[REGISTER] Username collected"
            << " | length = " << username.size() << '\n';

  printw("Password: ");
  refresh();

  std::string password = read_password(36);

  std::cerr << "[REGISTER] Password collected"
            << " | length = " << password.size() << '\n';

  std::string payload = username + "|" + password;

  auto register_packet = create_packet_stream(PacketType::Register, payload);

  queue_write(connection, register_packet);

  FlushResult write_status = flush_write(connection);

  if (write_status != FlushResult::QueueEmpty) {
    return RegisterStatus::ConnectionLost;
  }

  // Receive and interpret server response.
  std::size_t iteration = 0;

  while (true) {

    ++iteration;

    if (iteration > 6) {
      return RegisterStatus::ProtocolError;
    }

    std::string message;

    ProcessStreamResult result =
        process_data_stream(connection->rx_buffer, message);

    switch (result) {

    case ProcessStreamResult::Acknowledgement:

      if (message == "OK") {

        print_message("Successfully registered", PrintKind::Server);
        return RegisterStatus::Registered;
      }

      if (message == "NO") {
        return RegisterStatus::UserAlreadyExists;
      }

      return RegisterStatus::ProtocolError;

    case ProcessStreamResult::ErrorAck:

      if (message == "ERR") {
        return RegisterStatus::SQLErr;
      }

      return RegisterStatus::ProtocolError;

    case ProcessStreamResult::BufferEmpty:
    case ProcessStreamResult::IncompletePacket: {

      ReadResult read_status = handle_read(connection);

      switch (read_status) {

      case ReadResult::DataReceived:
        continue;

      case ReadResult::ConnectionClosed:
        return RegisterStatus::ConnectionLost;

      case ReadResult::FatalError:
        return RegisterStatus::ConnectionLost;

      case ReadResult::Ignored:
        return RegisterStatus::ConnectionLost;

      case ReadResult::WantRead:
        return RegisterStatus::ConnectionLost;

      case ReadResult::WantWrite:
        return RegisterStatus::ConnectionLost;
      }

      break;
    }

    case ProcessStreamResult::InvalidProtocolError:
      return RegisterStatus::ProtocolError;

    case ProcessStreamResult::PrintMessage:
      return RegisterStatus::ProtocolError;

    case ProcessStreamResult::ReceivedPong:
      return RegisterStatus::ProtocolError;
    }
  }
}
