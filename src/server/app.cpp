#include "authorization.hpp"
#include "network.hpp"
#include "protocol.hpp"
#include "server_connection.hpp"
#include "server_io.hpp"
#include "server_message_handler.hpp"
#include "sql_helper.hpp"
#include "ui.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#define SERVER_TIMER_IDENT 999

class ServerApp {
public:
  int listening_fd_;
  SSL_CTX *ssl_ctx_;

  ClientManager client_manager_;
  std::string console_input_buffer_;

private:
  bool p_running;
  int p_kq;
  std::vector<struct kevent> p_event_list;

private:
  SSL_CTX *init_server_ssl_context() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    if (!ctx) {
      ERR_print_errors_fp(stderr);
      exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <=
        0) {
      ERR_print_errors_fp(stderr);
      exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
      ERR_print_errors_fp(stderr);
      exit(EXIT_FAILURE);
    }

    if (!SSL_CTX_check_private_key(ctx)) {
      std::cerr << "Private key does not match public certificate\n";
      exit(EXIT_FAILURE);
    }

    return ctx;
  }

  void handle_login_case(VerifyClientStatus status,
                         ClientConnection *connection) {

    switch (status) {
    case VerifyClientStatus::Authorized: {
      connection->status = ConnectionStatus::Authenticated;
      auto auth_packet = create_packet_stream(PacketType::Login, "OK");
      queue_write(connection->fd, auth_packet, connection->tx_queue);
      break;
    }
    case VerifyClientStatus::NotAuthorized: {
      auto error_packet = create_packet_stream(PacketType::Login, "NO");
      queue_write(connection->fd, error_packet, connection->tx_queue);
      client_manager_.disconnect(connection);
      break;
    }
    case VerifyClientStatus::UserNotFound: {
      auto error_packet = create_packet_stream(PacketType::Login, "NO");
      queue_write(connection->fd, error_packet, connection->tx_queue);
      client_manager_.disconnect(connection);
      break;
    }
    case VerifyClientStatus::ExecFailed: {
      auto error_packet = create_packet_stream(PacketType::Login, "NO");
      queue_write(connection->fd, error_packet, connection->tx_queue);
      client_manager_.disconnect(connection);
      break;
    }
    }
  }

  void handle_register_client_status(RegisterClientStatus status,
                                     ClientConnection *connection) {

    switch (status) {
    case RegisterClientStatus::ExecFailed: {
      client_manager_.disconnect(connection);
      break;
    }
    case RegisterClientStatus::ClientRegistered: {
      auto ack_packet = create_packet_stream(PacketType::Register, "OK");
      queue_write(connection->fd, ack_packet, connection->tx_queue);
      break;
    }
    case RegisterClientStatus::UnableToOpenDatabase: {
      client_manager_.disconnect(connection);
      // log error, disconnect
      break;
    }
    case RegisterClientStatus::UsernameExists: {
      client_manager_.disconnect(connection);
      // log error, disconnect
      break;
    }
    default: {
      client_manager_.disconnect(connection);
      // unknown error, disconnect
      break;
    }
    }
  }

  void handle_handshake_client(HandshakeResult result,
                               ClientConnection *connection) {

    switch (result.request.kind) {
    case HandshakeKind::Login: {
      VerifyClientStatus status =
          verify_client(result.request.username, result.request.password);

      handle_login_case(status, connection);

      break;
    }
    case HandshakeKind::Reconnect: {
      if (verify_client_session(result.request.username,
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
      status =
          register_client(result.request.username, result.request.password);

      handle_register_client_status(status, connection);

      break;
    }
    case HandshakeKind::Error: {
      client_manager_.disconnect(connection);
      break;
    }
    }
  }

  void handle_accept_client_status(AcceptClientStatus status) {

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

  void handle_handshake_stream_status(HandshakeResult handshake_result,
                                      ClientConnection *connection) {

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
      client_manager_.disconnect(connection);
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

  void handle_process_stream_result(const ProcessStreamResult &result,
                                    ClientSession *session,
                                    std::string message) {

    switch (result) {
    case ProcessStreamResult::IncompletePacket: {
      return;
    }
    case ProcessStreamResult::InvalidProtocolError: {
      std::string failure_msg =
          "Packet sent with invalid protocol, Disconnecting " +
          session->username;
      print_message(failure_msg, PrintKind::Error);
      return;
    }
    case ProcessStreamResult::DisconnectClient: {
      client_manager_.disconnect(session->connection);
      return;
    }
    case ProcessStreamResult::SendPong: {
      client_manager_.pong(session->connection);
      return;
    }
    case ProcessStreamResult::PrintMessage: {
      auto packet = create_packet_stream(PacketType::Message, message);
      broadcast(session->connection->fd, packet, client_manager_);
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

  void handle_read_result(ClientConnection *connection, ReadResult result) {

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
      client_manager_.enable_write_event(p_kq, connection->fd);

      break;
    }
    }
  }

  void read_event(ClientConnection *connection) {
    if (connection == nullptr) {
      return;
    }

    if (connection->read_waiting_for_write &&
        connection->write_waiting_for_read) {
      // Invalid state: clean up or report the internal error.
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

  void write_event(ClientConnection *connection) {
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

  void console_write_event() {

    ConsoleInputResult result = read_console_input(console_input_buffer_);
    handle_console_input_result(result);
  }

  void handle_kick_status(KickStatus status, const std::string &username) {
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

  void handle_console_input_result(ConsoleInputResult result) {
    switch (result.status) {
    case ConsoleInputStatus::Message: {
      auto broad_pkt = create_packet_stream(PacketType::Message, result.buffer);
      broadcast(-1, broad_pkt, client_manager_);
      print_message(result.buffer, PrintKind::Self);
      return;
    }
    case ConsoleInputStatus::InvalidCommand: {
      std::string err_msg = "Invalid Command! : <" + result.buffer + ">";
      print_message(err_msg, PrintKind::Error);
      return;
    }
    case ConsoleInputStatus::KickClient: {
      KickStatus status = client_manager_.kick(result.buffer);
      handle_kick_status(status, result.buffer);
      return;
    }
    case ConsoleInputStatus::Quit: {
      std::string quit_msg = "Quitting...";
      print_message(quit_msg, PrintKind::System);
      p_running = false;
      return;
    }
    case ConsoleInputStatus::NoInput: {
      return;
    }
    }
  }

  void handle_flush_result(FlushResult result, ClientConnection *connection) {
    switch (result) {
    case FlushResult::FatalError: {
      client_manager_.disconnect(connection);
      return;
    }
    case FlushResult::QueueEmpty: {
      return;
    }
    case FlushResult::ReadWait: {
      connection->write_waiting_for_read = true;
      return;
    }
    case FlushResult::WriteWait: {
      return;
    }
    }
  }

  void client_connection_event() {

    AcceptClientStatus status =
        client_manager_.accept_client(listening_fd_, ssl_ctx_, p_kq);

    handle_accept_client_status(status);
  }

  void timer_event() {

    client_manager_.mark_timed_out_connections();

    client_manager_.remove_timed_out_connections();

    client_manager_.mark_expired_sessions();

    client_manager_.remove_expired_sessions();
  }

public:
  int initialize() {
    signal(SIGPIPE, SIG_IGN);

    this->listening_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;

    setsockopt(this->listening_fd_, SOL_SOCKET, SO_REUSEADDR, &opt,
               sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(this->listening_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("bind");
      return -1;
    }

    listen(this->listening_fd_, SOMAXCONN);

    ssl_ctx_ = init_server_ssl_context();

    setupNcurses();
    std::string msg = "Waiting for clients to join";
    print_message(msg, PrintKind::Server);

    set_nonblocking(this->listening_fd_);
    set_nonblocking(STDIN_FILENO);

    p_kq = kqueue();
    struct kevent init_evs[2];
    struct kevent timer_ev;

    EV_SET(&init_evs[0], this->listening_fd_, EVFILT_READ, EV_ADD | EV_ENABLE,
           0, 0, nullptr);
    EV_SET(&init_evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);
    kevent(p_kq, init_evs, 2, nullptr, 0, nullptr);

    EV_SET(&timer_ev, SERVER_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE,
           NOTE_SECONDS, 5, nullptr);
    kevent(p_kq, &timer_ev, 1, nullptr, 0, nullptr);

    msg = "[SERVER] active on PORT: 8080";
    print_message(msg, PrintKind::Server);

    p_event_list.reserve(32);

    create_table();

    std::string input_buffer;
    p_running = true;

    return 1;
  }

  void run() {
    while (p_running) {
      int nevents =
          kevent(p_kq, nullptr, 0, p_event_list.data(), MAX_EVENTS, nullptr);
      if (nevents < 0) {
        if (errno == EINTR)
          continue; // call interupted by async signal
        break;
      }

      for (int i = 0; i < nevents; ++i) {
        int current_fd = static_cast<int>(p_event_list[i].ident);

        if (p_event_list[i].filter == EVFILT_TIMER &&
            current_fd == SERVER_TIMER_IDENT) {

          timer_event();

          continue;
        }

        if (current_fd == STDIN_FILENO &&
            p_event_list[i].filter == EVFILT_READ) {
          console_write_event();
        }

        else if (p_event_list[i].filter == EVFILT_READ &&
                 current_fd == listening_fd_) {
          client_connection_event();
        }

        else if (p_event_list[i].filter == EVFILT_READ) {
          ClientConnection *connection =
              client_manager_.find_connection(current_fd);

          read_event(connection);
        }

        else if (p_event_list[i].filter == EVFILT_WRITE) {
          ClientConnection *connection =
              client_manager_.find_connection(current_fd);

          write_event(connection);
        }
      }
    }
  }
};
