#include "client/app.hpp"

SSL_CTX *ClientApp::init_client_ssl_context() {

  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();

  const SSL_METHOD *method = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);

  if (!ctx) {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }
  return ctx;
}

bool ClientApp::init_serveraddr() {
  std::string ip = read_server_ip();

  p_server_addr.sin_family = AF_INET;
  p_server_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, ip.c_str(), &p_server_addr.sin_addr) != 1) {
    // Invalid address
    return false;
  }

  return true;
}

bool ClientApp::handle_establish_connection_status(
    EstablishConnectionStatus status) {
  switch (status) {
  case EstablishConnectionStatus::ConnectionFailed: {
    return false;
    break;
  }
  case EstablishConnectionStatus::HandshakeFailed: {
    return false;
    break;
  }
  case EstablishConnectionStatus::Established: {
    return true;
  }
  }
}

DisconnectStatus ClientApp::disconnect() {

  auto &connection = p_manager.connection;

  if (connection.fd < 0 && connection.ssl == nullptr) {
    p_state = SessionState::Disconnected;
    return DisconnectStatus::AlreadyDisconnected;
  }

  p_manager.close_connection();

  p_state = SessionState::Disconnected;

  return DisconnectStatus::Disconnected;
}

LoginAction ClientApp::handle_login_result(LoginResult result) {

  switch (result.status) {

  case LoginStatus::AuthenticationRejected:
    return LoginAction::RetryLogin;
    break;
  case LoginStatus::ConnectionLost:
    return LoginAction::RetryLogin;
    break;
  case LoginStatus::ProtocolError:
    return LoginAction::Abort;
    break;
  case LoginStatus::Success:
    p_state = SessionState::Connected;
    p_manager.connection.status = ConnectionStatus::Authenticated;

    p_username = result.username;
    p_token = result.token;

    return LoginAction::EnterChat;
    break;
  }
}

bool ClientApp::perform_login_action(LoginAction action) {

  while (true) {

    switch (action) {

    case LoginAction::EnterChat:
      if (init_chat())
        return true;
      return false;

    case LoginAction::Abort:
      disconnect();
      std::cout << "LOGIN FAILED!" << std::endl;
      return false;

    case LoginAction::RetryLogin: {
      disconnect();
      EstablishConnectionStatus status =
          p_manager.establish_connection(ssl_ctx_, p_server_addr);

      if (!handle_establish_connection_status(status))
        return false;

      LoginResult result = login_client(&p_manager.connection);
      action = handle_login_result(result);
      continue;
    }
    }
  }
}

void ClientApp::enable_write_event() {
  struct kevent ev{};
  EV_SET(&ev, p_manager.connection.fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  kevent(p_kq, &ev, 1, nullptr, 0, nullptr);
}

void ClientApp::disable_write_event() {
  struct kevent ev{};
  EV_SET(&ev, p_manager.connection.fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
  kevent(p_kq, &ev, 1, nullptr, 0, nullptr);
}

bool ClientApp::init_chat() {

  auto &connection = p_manager.connection;

  if (connection.fd < 0 || !connection.ssl)
    return false;

  if (connection.status != ConnectionStatus::Authenticated)
    return false;

  connection.last_server_activity = ClientConnection::Clock::now();

  if (!set_nonblocking(connection.fd))
    return false;

  if (!set_nonblocking(STDIN_FILENO))
    return false;

  p_kq = kqueue();

  if (p_kq < 0)
    return false;

  nodelay(inputWin, true);

  struct kevent evs[3];

  EV_SET(&evs[0], connection.fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);

  EV_SET(&evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);

  EV_SET(&evs[2], CLIENT_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE,
         NOTE_SECONDS, 5, nullptr);

  if (kevent(p_kq, evs, 3, nullptr, 0, nullptr) < 0) {
    close(p_kq);
    p_kq = -1;
    return false;
  }

  p_event_list.resize(MAX_EVENTS);

  p_running = true;

  return true;
}

bool ClientApp::handle_process_stream_result(ProcessStreamResult result,
                                             const std::string &message) {
  switch (result) {

  case ProcessStreamResult::BufferEmpty:
  case ProcessStreamResult::IncompletePacket:
    return true;

  case ProcessStreamResult::InvalidProtocolError:
    print_message("Invalid server packet", PrintKind::Error);
    disconnect();
    return false;

  case ProcessStreamResult::ReceivedPong:
    p_manager.connection.last_server_activity = ClientConnection::Clock::now();
    return true;

  case ProcessStreamResult::PrintMessage:
    print_message(message, PrintKind::Self);
    return true;

  case ProcessStreamResult::Acknowledgement:
    print_message(message, PrintKind::Server);
    return true;

  case ProcessStreamResult::ErrorAck:
    print_message(message, PrintKind::Error);
    return true;
  }

  return false;
}

void ClientApp::handle_read_result(ReadResult result) {

  auto &connection = p_manager.connection;

  switch (result) {
  case ReadResult::DataReceived: {
    process_received_data();
    break;
  }
  case ReadResult::FatalError: {
    std::string error_msg = "Connection read failed!.";
    print_message(error_msg, PrintKind::Error);
    disconnect();
    return;
  }
  case ReadResult::WantRead: {
    break;
  }
  case ReadResult::WantWrite: {
    connection.read_waiting_for_write = true;
    enable_write_event();

    break;
  }
  case ReadResult::ConnectionClosed:
    print_message("Server disconnected.", PrintKind::Error);
    disconnect();
    p_running = false;
    return;

  case ReadResult::Ignored:
    return;
  }
}

void ClientApp::process_received_data() {

  auto &connection = p_manager.connection;

  while (true) {

    std::string message;

    ProcessStreamResult result =
        process_data_stream(connection.rx_buffer, message);

    if (!handle_process_stream_result(result, message)) {
      return;
    }

    if (result == ProcessStreamResult::BufferEmpty ||
        result == ProcessStreamResult::IncompletePacket) {
      return;
    }
  }
}

void ClientApp::handle_flush_result(FlushResult result) {

  auto &connection = p_manager.connection;

  switch (result) {

  case FlushResult::QueueEmpty:

    if (connection.status == ConnectionStatus::Closing) {
      disconnect();
      return;
    }

    if (!connection.read_waiting_for_write)
      disable_write_event();

    return;

  case FlushResult::ReadWait:

    connection.write_waiting_for_read = true;
    disable_write_event();
    return;

  case FlushResult::WriteWait:

    enable_write_event();
    return;

  case FlushResult::FatalError:

    disconnect();
    p_running = false;
    return;
  }
}

void ClientApp::handle_timer_event() {

  auto &connection = p_manager.connection;

  auto now = ClientConnection::Clock::now();

  if (now - connection.last_server_activity >= std::chrono::seconds(10)) {

    print_message("Server heartbeat timeout.", PrintKind::Error);

    disconnect();
    p_running = false;
    return;
  }

  auto ping = create_packet_stream(PacketType::Ping, "");

  bool was_empty = connection.tx_queue.empty();

  queue_write(&connection, ping);

  if (was_empty)
    enable_write_event();
}

void ClientApp::read_event() {
  auto &connection = p_manager.connection;

  if (connection.read_waiting_for_write && connection.write_waiting_for_read) {
    // Invalid state: clean up or report the internal error.
    return;
  }

  if (connection.read_waiting_for_write) {
    return;
  }

  if (connection.write_waiting_for_read) {
    connection.write_waiting_for_read = false;

    FlushResult result = flush_write(&connection);
    handle_flush_result(result);
    return;
  }

  ReadResult result = handle_read(&connection);
  handle_read_result(result);
}

void ClientApp::write_event() {

  auto &connection = p_manager.connection;

  if (connection.read_waiting_for_write && connection.write_waiting_for_read) {
    // Invalid state: clean up or report the internal error.
    return;
  }

  if (connection.write_waiting_for_read) {
    return;
  }

  if (connection.read_waiting_for_write) {
    connection.read_waiting_for_write = false;

    ReadResult result = handle_read(&connection);
    handle_read_result(result);
    return;
  }

  FlushResult result = flush_write(&connection);
  handle_flush_result(result);
}

void ClientApp::handle_console_input_result(ConsoleInputResult result) {
  switch (result.status) {
  case ConsoleInputStatus::Message: {

    auto &connection = p_manager.connection;
    auto broad_pkt = create_packet_stream(PacketType::Message, result.buffer);

    bool was_empty = connection.tx_queue.empty();

    queue_write(&connection, broad_pkt);

    if (was_empty)
      enable_write_event();

    print_message(result.buffer, PrintKind::Self);

    return;
  }
  case ConsoleInputStatus::InvalidCommand: {
    std::string err_msg = "Invalid Command! : <" + result.buffer + ">";
    print_message(err_msg, PrintKind::Error);
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

void ClientApp::console_write_event() {

  ConsoleInputResult result = read_console_input(console_input_buffer_);
  handle_console_input_result(result);
}

bool ClientApp::handle_register_status(RegisterStatus status) {

  switch (status) {

  case RegisterStatus::Registered:
    return true;

  case RegisterStatus::UserAlreadyExists:
    print_message("Username already exists.", PrintKind::Error);
    break;

  case RegisterStatus::SQLErr:
    print_message("Server database error.", PrintKind::Error);
    break;

  case RegisterStatus::ProtocolError:
    print_message("Invalid server response.", PrintKind::Error);
    break;

  case RegisterStatus::ConnectionLost:
    print_message("Connection lost during registration.", PrintKind::Error);
    break;
  }

  disconnect();
  return false;
}

bool ClientApp::initialize() {

  setupNcurses();

  signal(SIGPIPE, SIG_IGN);

  ssl_ctx_ = init_client_ssl_context();

  if (!ssl_ctx_) {
    return false;
  }

  if (!init_serveraddr()) {
    return false;
  }

  EstablishConnectionStatus status =
      p_manager.establish_connection(ssl_ctx_, p_server_addr);

  if (!handle_establish_connection_status(status)) {
    return false;
  }

  printw("REEGISTER OR LOGIN\n");
  printw("1 - REGISTER\n2 - LOGIN\n");

  refresh();

  int ch;
  ch = getch();

  if (ch == '1' || ch == 'r' || ch == 'R') {

    RegisterStatus status = register_client(&p_manager.connection);
    if (!handle_register_status(status)) {

      return false;
    }
  }

  LoginResult result = login_client(&p_manager.connection);

  LoginAction action = handle_login_result(result);

  return perform_login_action(action);
}

void ClientApp::run() {
  while (p_running) {
    int nevents =
        kevent(p_kq, nullptr, 0, p_event_list.data(), MAX_EVENTS, nullptr);
    if (nevents < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    for (int i = 0; i < nevents; ++i) {
      int current_fd = static_cast<int>(p_event_list[i].ident);

      if (p_event_list[i].filter == EVFILT_TIMER &&
          current_fd == CLIENT_TIMER_IDENT) {
        handle_timer_event();
      }
      if (current_fd == STDIN_FILENO) {
        console_write_event();
      } else if (p_event_list[i].filter == EVFILT_READ) {
        read_event();
      } else if (p_event_list[i].filter == EVFILT_WRITE) {
        write_event();
      }
    }
  }
}

void ClientApp::end() {

  p_running = false;

  if (p_kq >= 0) {
    close(p_kq);
    p_kq = -1;
  }

  disconnect();

  if (ssl_ctx_) {
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = nullptr;
  }

  cleanupNcurses();
}

ClientApp::ClientApp() {}

ClientApp::~ClientApp() { end(); }
