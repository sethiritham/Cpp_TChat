#include "client/authorization.hpp"
#include "client/connection.hpp"
#include "client/io.hpp"
#include "client/message_handler.hpp"
#include "common/network.hpp"
#include "common/protocol.hpp"
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

    auto process_status = process_client_stream(connection->rx_buffer, msg);

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
