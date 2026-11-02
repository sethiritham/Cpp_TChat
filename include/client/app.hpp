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

  void handle_establish_connection_status(EstablishConnectionStatus status) {
    switch (status) {
    case EstablishConnectionStatus::ConnectionFailed: {
      // end the app, log error
      break;
    }
    case EstablishConnectionStatus::HandshakeFailed: {
      // retry handshake?
      break;
    }
    case EstablishConnectionStatus::Established: {
      // connection established, move onto authentication?
    }
    }
  }

  int handle_login_result(LoginResult result) {

    switch (result.status) {

    case LoginStatus::AuthenticationRejected:
      // disconnect
      //  What do I do?
      return 0;
      break;
    case LoginStatus::ConnectionLost:
      // disconnect
      // What do I do?
      return 0;
      break;
    case LoginStatus::ProtocolError:
      // disconnect
      //  What do I do?
      return 0;
      break;
    case LoginStatus::Success:
      // Do nothing
      return 1;
      break;
    }
  }

public:
  int initialize() {

    ssl_ctx_ = init_client_ssl_context();

    if (!init_serveraddr())
      return -1;

    EstablishConnectionStatus status =
        p_manager.establish_connection(ssl_ctx_, p_server_addr);
    handle_establish_connection_status(status);

    LoginResult result = login_client(&p_manager.connection);

    if (!handle_login_result(result))
      return 0;

    p_manager.connection.last_server_activity = ClientConnection::Clock::now();

    setupNcurses();
    nodelay(inputWin, true);

    signal(SIGPIPE, SIG_IGN);

    set_nonblocking(listening_fd_);
    set_nonblocking(STDIN_FILENO);

    p_kq = kqueue();

    struct kevent evs[3];

    EV_SET(&evs[0], listening_fd_, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);
    EV_SET(&evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);
    EV_SET(&evs[2], CLIENT_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE,
           NOTE_SECONDS, 5, nullptr);

    int kevent_error = 0;
    kevent_error = kevent(p_kq, evs, 3, nullptr, 0, nullptr);

    if (kevent_error < 0) {
      perror("kevent registration failed");
    }

    std::string input_buffer;
    std::vector<uint8_t> rx_buffer;

    p_event_list.reserve(32);

    p_running = true;

    return 1;
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
          // read event
        } else if (p_event_list[i].filter == EVFILT_WRITE) {
          // write event
        }
      }
    }
  }

  void end() {
    cleanupNcurses();

    close(listening_fd_);
  }
};
