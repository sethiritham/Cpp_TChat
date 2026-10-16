#include "auth.hpp"
#include "chat2.hpp"
#include "network.hpp"
#include "server_connection.hpp"
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

private:
  bool p_running;
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
    safePrint("Waiting for clients to join", true);

    set_nonblocking(this->listening_fd_);
    set_nonblocking(STDIN_FILENO);

    int kq = kqueue();
    struct kevent init_evs[2];
    struct kevent timer_ev;

    EV_SET(&init_evs[0], this->listening_fd_, EVFILT_READ, EV_ADD | EV_ENABLE,
           0, 0, nullptr);
    EV_SET(&init_evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           nullptr);
    kevent(kq, init_evs, 2, nullptr, 0, nullptr);

    EV_SET(&timer_ev, SERVER_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE,
           NOTE_SECONDS, 5, nullptr);
    kevent(kq, &timer_ev, 1, nullptr, 0, nullptr);

    safePrint("[SERVER] active on PORT: 8080", true);

    p_event_list.reserve(32);

    if (!create_table()) {
      return -1;
    }

    std::string input_buffer;
    p_running = true;

    return 1;
  }
};
