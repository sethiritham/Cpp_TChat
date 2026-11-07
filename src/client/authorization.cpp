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

    auto process_result = process_data_stream(connection->rx_buffer);

    if (process_result.status == ProcessStreamStatus::IncompletePacket)
      continue;

    if (process_result.status == ProcessStreamStatus::InvalidProtocolError) {
      result.status = LoginStatus::ProtocolError;
      return result;
    }

    if (process_result.message == "NO") {
      result.status = LoginStatus::AuthenticationRejected;
      return result;
    }

    if (process_result.message.rfind("OK|", 0) == 0 &&
        process_result.message.size() > 3) {
      result.username = username;
      result.token = process_result.message.substr(3);
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

  printw("Password: ");
  refresh();

  std::string password = read_password(36);

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

    ProcessStreamResult result = process_data_stream(connection->rx_buffer);

    switch (result.status) {

    case ProcessStreamStatus::Acknowledgement:

      if (message == "OK") {

        print_message("Successfully registered", PrintKind::Server);
        return RegisterStatus::Registered;
      }

      if (message == "NO") {
        return RegisterStatus::UserAlreadyExists;
      }

      return RegisterStatus::ProtocolError;

    case ProcessStreamStatus::ErrorAck:

      if (message == "ERR") {
        return RegisterStatus::SQLErr;
      }

      return RegisterStatus::ProtocolError;

    case ProcessStreamStatus::BufferEmpty:
    case ProcessStreamStatus::IncompletePacket: {

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

    case ProcessStreamStatus::InvalidProtocolError:
      return RegisterStatus::ProtocolError;

    case ProcessStreamStatus::PrintMessage:
      return RegisterStatus::ProtocolError;

    case ProcessStreamStatus::ReceivedPong:
      return RegisterStatus::ProtocolError;
    }
  }
}
