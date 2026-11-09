#include "server/event_handler.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include <iostream>
#include <random>

#define SERVER_TIMER_IDENT 999

std::string generate_token() {
  const std::string characters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

  int length = 8;

  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<> distribution(0, characters.size() - 1);

  std::string random_string;
  for (size_t i = 0; (int)i < length; ++i) {
    random_string += characters[distribution(generator)];
  }

  return random_string;
}

void EventHandler::set_kq(int kq) { p_kq = kq; }

void EventHandler::handle_login_case(VerifyClientStatus status,
                                     ClientConnection *connection,
                                     std::string username) {

  if (!connection) {
    return;
  }

  switch (status) {

  case VerifyClientStatus::Authorized: {
    print_message("Client authorized, sending ack", PrintKind::Server);

    ClientSession session;
    connection->status = ConnectionStatus::Authenticated;
    connection->last_seen = ClientConnection::Clock::now();
    auto token = generate_token();
    auto auth_packet = create_packet_stream(PacketType::Ack, "OK|" + token);

    session.username = username;
    session.token = token;

    session.connection = connection;
    session.state = SessionState::Connected;

    client_manager_.add_session(session);

    queue_write(connection, auth_packet);
    enable_write_event(p_kq, connection->fd);

    std::string join_msg = username + " has joined the chat!";
    std::string join_payload = "S|" + join_msg;

    auto join_pkt = create_packet_stream(PacketType::Message, join_payload);

    auto write_fds = broadcast(connection->fd, join_pkt, client_manager_);

    for (int fd : write_fds) {
      enable_write_event(p_kq, fd);
    }

    print_message(join_msg, PrintKind::Server);

    break;
  }

  case VerifyClientStatus::NotAuthorized: {
    auto error_packet = create_packet_stream(PacketType::Ack, "NO");
    queue_write(connection, error_packet);
    connection->status = ConnectionStatus::Closing;
    enable_write_event(p_kq, connection->fd);
    break;
  }

  case VerifyClientStatus::UserNotFound: {
    auto error_packet = create_packet_stream(PacketType::Ack, "NO");
    queue_write(connection, error_packet);
    connection->status = ConnectionStatus::Closing;
    enable_write_event(p_kq, connection->fd);
    break;
  }

  case VerifyClientStatus::ExecFailed: {

    auto error_packet = create_packet_stream(PacketType::Error, "ERR");
    queue_write(connection, error_packet);
    connection->status = ConnectionStatus::Closing;
    enable_write_event(p_kq, connection->fd);
    break;
  }
  }
}

void EventHandler::handle_register_client_status(RegisterClientStatus status,
                                                 ClientConnection *connection) {

  switch (status) {
  case RegisterClientStatus::ExecFailed: {

    print_message("SQL execution failed!", PrintKind::Server);
    client_manager_.disconnect(connection);
    break;
  }
  case RegisterClientStatus::ClientRegistered: {

    print_message("Client has been registered", PrintKind::Server);

    connection->last_seen = ClientConnection::Clock::now();

    auto ack_packet = create_packet_stream(PacketType::Ack, "OK");

    queue_write(connection, ack_packet);

    enable_write_event(p_kq, connection->fd);

    break;
  }
  case RegisterClientStatus::UnableToOpenDatabase: {

    print_message("Unable to open database", PrintKind::Error);
    client_manager_.disconnect(connection);
    break;
  }
  case RegisterClientStatus::UsernameExists: {
    print_message("Username already exists, unable to register client",
                  PrintKind::Error);
    client_manager_.disconnect(connection);
    // log error, disconnect
    break;
  }
  default: {
    print_message("Unable to register client, reason unknown",
                  PrintKind::Error);
    client_manager_.disconnect(connection);
    // unknown error, disconnect
    break;
  }
  }
}

void EventHandler::handle_handshake_client(HandshakeResult result,
                                           ClientConnection *connection) {

  switch (result.request.kind) {
  case HandshakeKind::Login: {
    VerifyClientStatus status =
        verify_client(result.request.username, result.request.password);

    handle_login_case(status, connection, result.request.username);

    break;
  }
  case HandshakeKind::Reconnect: {
    if (client_manager_.verify_session(result.request.username,
                                       result.request.token)) {
      client_manager_.reconnect_client(result.request.username, connection);
    } else {
      // log reconnect faliure?
      client_manager_.disconnect(connection);
    }
    break;
  }
  case HandshakeKind::Register: {
    RegisterClientStatus status;
    status = register_client(result.request.username, result.request.password);

    handle_register_client_status(status, connection);

    break;
  }
  case HandshakeKind::Error: {
    client_manager_.disconnect(connection);
    break;
  }
  }
}

void EventHandler::handle_accept_client_status(AcceptClientStatus status) {

  switch (status) {
  case AcceptClientStatus::FailedHandshake: {
    std::string faliure_msg = "TLS Handshake failed, unable to "
                              "accept the client, disconnecting";
    print_message(faliure_msg, PrintKind::Error);
    break;
  }
  case AcceptClientStatus::AcceptedClient: {
    std::string success_msg = "Handshake sucessfull, Client connected!";
    print_message(success_msg, PrintKind::Server);
    break;
  }
  case AcceptClientStatus::NoPendingConnection: {
    break;
  }
  }
}

void EventHandler::handle_handshake_stream_status(
    HandshakeResult handshake_result, ClientConnection *connection) {

  HandshakeStreamStatus status = handshake_result.status;
  switch (status) {
  case HandshakeStreamStatus::IncompletePacket: {
    break;
  }
  case HandshakeStreamStatus::InvalidProtocolError: {
    std::string faliure_msg =
        "Auth packet sent with the wrong protocol, authentication "
        "failed, disconnecting";
    print_message(faliure_msg, PrintKind::Error);
    client_manager_.disconnect(connection);
    break;
  }
  case HandshakeStreamStatus::InvalidAuthPacket: {
    std::string faliure_msg = "Invalid auth packet sent, authentication "
                              "failed, disconnecting";
    print_message(faliure_msg, PrintKind::Error);
    client_manager_.disconnect(connection);
    break;
  }
  case HandshakeStreamStatus::ValidAuthPacket: {
    handle_handshake_client(handshake_result, connection);
    break;
  }
  default: {
    std::string faliure_msg =
        "Client unable to connnect due to an unknown error!";
    print_message(faliure_msg, PrintKind::Error);
    client_manager_.disconnect(connection);
    break;
  }
  }
}

void EventHandler::handle_process_stream_result(
    const ProcessStreamResult &result, ClientSession *session,
    std::string message) {

  switch (result) {
  case ProcessStreamResult::IncompletePacket: {
    return;
  }
  case ProcessStreamResult::InvalidProtocolError: {
    std::string failure_msg =
        "Packet sent with invalid protocol, Disconnecting " + session->username;
    print_message(failure_msg, PrintKind::Error);
    return;
  }
  case ProcessStreamResult::DisconnectClient: {
    client_manager_.disconnect(session->connection);
    return;
  }
  case ProcessStreamResult::SendPong: {
    if (!session) {
      std::cerr << "[ERROR] Received PING but session is nullptr\n";
      return;
    }

    if (!session->connection) {
      std::cerr << "[ERROR] Received PING but session has no connection\n";
      return;
    }
    client_manager_.pong(session->connection);
    enable_write_event(p_kq, session->connection->fd);
    return;
  }
  case ProcessStreamResult::PrintMessage: {
    std::string payload = "U|" + session->username + "|" + message;

    auto packet = create_packet_stream(PacketType::Message, payload);

    auto write_fds =
        broadcast(session->connection->fd, packet, client_manager_);

    for (int fd : write_fds) {
      enable_write_event(p_kq, fd);
    }

    print_message(message, PrintKind::Client, session->username);
    return;
  }
  case ProcessStreamResult::BufferEmpty: {
  }
  case ProcessStreamResult::ProcessComplete: {
    return;
  }
  }
}

void EventHandler::handle_read_result(ClientConnection *connection,
                                      ReadResult result) {

  switch (result) {
  case ReadResult::DataReceived: {
    if (connection->status == ConnectionStatus::AwaitingAuth) {
      HandshakeResult handshake_result;
      handshake_result = process_handshake_packet(connection->rx_buffer);

      handle_handshake_stream_status(handshake_result, connection);

    } else {

      ClientSession *session =
          client_manager_.find_sesssion_from_connection(connection);

      ProcessStreamResult result;
      std::string message;
      result = process_client_stream(connection->rx_buffer, message);

      handle_process_stream_result(result, session, message);
    }
    break;
  }
  case ReadResult::FatalError: {
    std::string error_msg = "Connection read failed!.";
    print_message(error_msg, PrintKind::Error);
    client_manager_.disconnect(connection);
    return;
  }
  case ReadResult::WantRead: {
    break;
  }
  case ReadResult::WantWrite: {
    connection->read_waiting_for_write = true;
    enable_write_event(p_kq, connection->fd);

    break;
  }
  case ReadResult::ConnectionClosed: {
    client_manager_.disconnect(connection);
    return;
  }

  case ReadResult::Ignored: {
    return;
  }
  }
}

void EventHandler::read_event(ClientConnection *connection) {
  if (connection == nullptr) {
    return;
  }

  if (connection->read_waiting_for_write &&
      connection->write_waiting_for_read) {
    return;
  }

  if (connection->read_waiting_for_write) {
    return;
  }

  if (connection->write_waiting_for_read) {
    connection->write_waiting_for_read = false;

    FlushResult result = flush_write(connection);
    handle_flush_result(result, connection);
    return;
  }

  ReadResult result = handle_read(connection);
  handle_read_result(connection, result);
}

void EventHandler::write_event(ClientConnection *connection) {
  if (connection == nullptr) {
    return;
  }

  if (connection->read_waiting_for_write &&
      connection->write_waiting_for_read) {
    // Invalid state: clean up or report the internal error.
    return;
  }

  if (connection->write_waiting_for_read) {
    return;
  }

  if (connection->read_waiting_for_write) {
    connection->read_waiting_for_write = false;

    ReadResult result = handle_read(connection);
    handle_read_result(connection, result);
    return;
  }

  FlushResult result = flush_write(connection);
  handle_flush_result(result, connection);
}

bool EventHandler::console_write_event() {

  ConsoleInputResult result = read_console_input(console_input_buffer_);

  return handle_console_input_result(result);
}

void EventHandler::handle_kick_status(KickStatus status,
                                      const std::string &username) {
  switch (status) {
  case KickStatus::ClientNotFound: {
    std::string err_msg = "Client: " + username + " not found!";
    print_message(err_msg, PrintKind::Error);
    return;
  }
  case KickStatus::Kicked: {
    std::string kick_msg = "Kicked client " + username;
    print_message(kick_msg, PrintKind::Server);
    return;
  }
  }
}

bool EventHandler::handle_console_input_result(ConsoleInputResult result) {
  switch (result.status) {
  case ConsoleInputStatus::Message: {
    std::string payload = "S|" + result.buffer;

    auto broad_pkt = create_packet_stream(PacketType::Message, payload);

    auto write_fds = broadcast(-1, broad_pkt, client_manager_);

    for (int fd : write_fds) {
      enable_write_event(p_kq, fd);
    }
    print_message(result.buffer, PrintKind::Self);
    return true;
  }
  case ConsoleInputStatus::InvalidCommand: {
    std::string err_msg = "Invalid Command! : <" + result.buffer + ">";
    print_message(err_msg, PrintKind::Error);
    return true;
  }
  case ConsoleInputStatus::KickClient: {
    KickStatus status = client_manager_.kick(result.buffer);
    handle_kick_status(status, result.buffer);
    return true;
  }
  case ConsoleInputStatus::Quit: {
    std::string quit_msg = "Quitting...";
    print_message(quit_msg, PrintKind::System);
    return false;
  }
  case ConsoleInputStatus::NoInput: {
    return true;
  }
  }
}

void handle_disconnect_status(DisconnectStatus status) {
  switch (status) {
  case DisconnectStatus::Disconnected:
    return;
  case DisconnectStatus::ClientNotFound:
    print_message("Client not found!\n", PrintKind::Error);
    return;
  }
}

void EventHandler::handle_flush_result(FlushResult result,
                                       ClientConnection *connection) {

  if (!connection)
    return;

  switch (result) {

  case FlushResult::FatalError: {

    DisconnectStatus status = client_manager_.disconnect(connection);

    handle_disconnect_status(status);
    return;
  }

  case FlushResult::QueueEmpty: {

    connection->write_waiting_for_read = false;

    if (connection->status == ConnectionStatus::Closing) {

      DisconnectStatus status = client_manager_.disconnect(connection);

      handle_disconnect_status(status);
      return;
    }

    // Keep EVFILT_WRITE if an SSL_read is waiting for it.
    if (!connection->read_waiting_for_write) {
      disable_write_event(p_kq, connection->fd);
    }

    return;
  }

  case FlushResult::ReadWait: {

    connection->write_waiting_for_read = true;

    // Wait for READ readiness before retrying SSL_write.
    disable_write_event(p_kq, connection->fd);

    return;
  }

  case FlushResult::WriteWait: {

    connection->write_waiting_for_read = false;

    // SSL_write needs WRITE readiness.
    enable_write_event(p_kq, connection->fd);

    return;
  }
  }
}
void EventHandler::client_connection_event(int listening_fd, SSL_CTX *ssl_ctx) {

  AcceptClientStatus status =
      client_manager_.accept_client(listening_fd, ssl_ctx, p_kq);

  handle_accept_client_status(status);
}

void EventHandler::timer_event() {

  client_manager_.mark_timed_out_connections();

  client_manager_.remove_timed_out_connections();

  client_manager_.mark_expired_sessions();

  client_manager_.remove_expired_sessions();
}

void EventHandler::enable_write_event(int kq, int fd) {
  struct kevent ev;

  EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);

  if (kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
    perror("Failed to enable write event");
  }
}

void EventHandler::disable_write_event(int kq, int fd) {
  struct kevent ev;

  EV_SET(&ev, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, nullptr);

  if (kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
    perror("Failed to disable write event");
  }
}
