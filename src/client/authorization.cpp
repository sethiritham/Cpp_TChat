#include "client/authorization.hpp"
#include "client/connection.hpp"
#include "client/io.hpp"
#include "client/message_handler.hpp"
#include "common/network.hpp"
#include "common/protocol.hpp"
#include <string>

LoginResult login_client(ClientConnection *connection) {
  printw("- - - LOGIN - - -\n");

  printw("Username: ");
  refresh();
  std::string username = read_username(36);

  printw("Password: ");
  refresh();
  std::string password = read_password(36);

  std::string login_payload = username + "|" + password;

  auto login_packet = create_packet_stream(PacketType::Login, login_payload);

  SendPacketStatus status = send_packet(login_packet, connection->ssl);

  LoginResult result;

  if (status != SendPacketStatus::PacketSent) {
    result.status = LoginStatus::ConnectionLost;
    return result;
  }

  receive_data(connection->ssl, connection->rx_buffer);

  std::string msg;
  process_client_stream(connection->rx_buffer, msg);

  if (msg != "OK") {
    result.status = LoginStatus::AuthenticationRejected;
    return result;
  }

  result.status = LoginStatus::Success;
  return result;
}
