#include "server/app.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include "server/manager.hpp"
#include <iostream>

#define SERVER_TIMER_IDENT 999

SSL_CTX *ServerApp::init_server_ssl_context() {
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();

  const SSL_METHOD *method = TLS_server_method();
  SSL_CTX *ctx = SSL_CTX_new(method);

  if (!ctx) {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
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

int ServerApp::initialize() {
  signal(SIGPIPE, SIG_IGN);

  this->listening_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;

  setsockopt(this->listening_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

  p_event_handler.set_kq(p_kq);

  EV_SET(&init_evs[0], this->listening_fd_, EVFILT_READ, EV_ADD | EV_ENABLE, 0,
         0, nullptr);
  EV_SET(&init_evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  kevent(p_kq, init_evs, 2, nullptr, 0, nullptr);

  EV_SET(&timer_ev, SERVER_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE,
         NOTE_SECONDS, 5, nullptr);
  kevent(p_kq, &timer_ev, 1, nullptr, 0, nullptr);

  msg = "Active on PORT: 8080";
  print_message(msg, PrintKind::Server);

  p_event_list.reserve(MAX_EVENTS);
  p_event_list.resize(MAX_EVENTS);

  create_user_database();

  std::string input_buffer;
  p_running = true;

  return 1;
}

void ServerApp::run() {
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

        p_event_handler.timer_event();

        continue;
      }

      if (current_fd == STDIN_FILENO && p_event_list[i].filter == EVFILT_READ) {
        p_event_handler.console_write_event();
      }

      else if (p_event_list[i].filter == EVFILT_READ &&
               current_fd == listening_fd_) {
        p_event_handler.client_connection_event(listening_fd_, ssl_ctx_);
      }

      else if (p_event_list[i].filter == EVFILT_READ) {
        ClientConnection *connection =
            p_event_handler.client_manager_.find_connection(current_fd);

        p_event_handler.read_event(connection);
      }

      else if (p_event_list[i].filter == EVFILT_WRITE) {
        ClientConnection *connection =
            p_event_handler.client_manager_.find_connection(current_fd);

        p_event_handler.write_event(connection);
      }
    }
  }
}

void ServerApp::end() {
  cleanupNcurses();
  close(p_kq);
  close(listening_fd_);
}
