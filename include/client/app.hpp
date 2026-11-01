#include "common/ui.hpp"
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

public:
  int initialize() {

    SSL_CTX *ssl_ctx = init_client_ssl_context();
    // int client_fd = client_login(ssl_ctx);

    setupNcurses();
    // g_last_server_activity = time(nullptr);
    nodelay(inputWin, true);

    // if (client_fd < 0) {
    //   perror("LOGIN ERROR");
    //   return -1;
    // }

    signal(SIGPIPE, SIG_IGN);

    // set_nonblocking(client_fd);
    // set_nonblocking(STDIN_FILENO);

    int kq = kqueue();

    struct kevent evs[3];

    // EV_SET(&evs[0], client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
    // nullptr); EV_SET(&evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE,
    // 0, 0,
    //        nullptr);
    // EV_SET(&evs[2], CLIENT_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE,
    //        NOTE_SECONDS, 5, nullptr);

    int kevent_error = 0;
    kevent_error = kevent(kq, evs, 3, nullptr, 0, nullptr);

    if (kevent_error < 0) {
      perror("kevent registration failed");
    }

    std::string input_buffer;
    std::vector<uint8_t> rx_buffer;
    // std::vector<struct kevent> event_list(MAX_EVENTS);

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
          // ping and timeout check
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
    close(p_kq);
    close(listening_fd_);
  }
};
