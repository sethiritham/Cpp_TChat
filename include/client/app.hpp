#include "authorization.hpp"
#include "common/network.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include "connection.hpp"
#include "io.hpp"
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#define CLIENT_TIMER_IDENT 999
#define MAX_EVENTS 15
#define PORT 8080

enum class SessionState {
  Connected,
  Reconnected,
  Kicked,
  Expired,
  Disconnected
};

enum class DisconnectStatus {
  Disconnected,
  FailedToDisconnect,
  AlreadyDisconnected
};

enum class LoginAction { EnterChat, RetryLogin, Abort };

class ClientApp {
public:
  int listening_fd_;
  SSL_CTX *ssl_ctx_;

  // some sort of connection handler ig?
  std::string console_input_buffer_;

private:
  bool p_running;
  int p_kq;
  std::vector<struct kevent> p_event_list;

  using Clock = std::chrono::steady_clock;
  sockaddr_in p_server_addr{};

  std::string p_username;
  std::string p_token;

  ConnectionManager p_manager;
  int p_num_reconnects = 0;

  SessionState p_state = SessionState::Connected;

private:
  SSL_CTX *init_client_ssl_context() {

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

  bool init_serveraddr() {
    // read serverIP - io.cpp
    std::string ip; // temp

    p_server_addr.sin_family = AF_INET;
    p_server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, ip.c_str(), &p_server_addr.sin_addr) != 1) {
      // Invalid address
      return false;
    }

    return true;
  }

  bool handle_establish_connection_status(EstablishConnectionStatus status) {
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

  DisconnectStatus disconnect() {
    auto &connection = p_manager.connection;

    if (connection.fd < 0 && connection.ssl == nullptr) {
      p_state = SessionState::Disconnected;
      return DisconnectStatus::AlreadyDisconnected;
    }

    p_manager.close_connection();

    p_state = SessionState::Disconnected;

    return DisconnectStatus::Disconnected;
  }

  LoginAction handle_login_result(LoginResult result) {

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

  bool perform_login_action(LoginAction action) {

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

  void enable_write_event() {
    struct kevent ev{};
    EV_SET(&ev, p_manager.connection.fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);
    kevent(p_kq, &ev, 1, nullptr, 0, nullptr);
  }

  void disable_write_event() {
    struct kevent ev{};
    EV_SET(&ev, p_manager.connection.fd, EVFILT_WRITE, EV_DELETE, 0, 0,
           nullptr);
    kevent(p_kq, &ev, 1, nullptr, 0, nullptr);
  }

  bool init_chat() {

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

    struct kevent evs[4];

    EV_SET(&evs[0], connection.fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);

    EV_SET(&evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);

    EV_SET(&evs[2], connection.fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);

    EV_SET(&evs[3], CLIENT_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE,
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

public:
  bool initialize() {

    ssl_ctx_ = init_client_ssl_context();

    if (!init_serveraddr())
      return false;

    EstablishConnectionStatus status =
        p_manager.establish_connection(ssl_ctx_, p_server_addr);

    if (!handle_establish_connection_status(status))
      return false;

    LoginResult result = login_client(&p_manager.connection);

    LoginAction action = handle_login_result(result);

    return perform_login_action(action);
  }

  void run() {
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

          auto ping_pkt = create_packet_stream(PacketType::Ping, "");
          queue_write(&p_manager.connection, ping_pkt);
        }

        if (current_fd == STDIN_FILENO) {
          // console input
        } else if (p_event_list[i].filter == EVFILT_READ) {
          // read_event(); : Handle write waiting for read case
        } else if (p_event_list[i].filter == EVFILT_WRITE) {
          // write_event() : handle read waiting for write case;
        }
      }
    }
  }

  void end() {
    cleanupNcurses();

    close(listening_fd_);
  }
};
