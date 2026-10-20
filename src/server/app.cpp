#include "network.hpp"
#include "protocol.hpp"
#include "server_connection.hpp"
#include "server_io.hpp"
#include "server_message_handler.hpp"
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

    // setupNcurses();
    // safePrint("Waiting for clients to join", true);

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

    // safePrint("[SERVER] active on PORT: 8080", true);

    p_event_list.reserve(32);

    // create database

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
          // check_client_connection();
          // Incase client is inactive for more than 10 sec, disconnect the
          // client
        }

        if (current_fd == STDIN_FILENO) {
          // handle keyboard read event
          // write
        }

        else if (p_event_list[i].filter == EVFILT_READ &&
                 current_fd == listening_fd_) {

          AcceptClientStatus status =
              client_manager_.accept_client(listening_fd_, ssl_ctx_, p_kq);

          switch (status) {
          case AcceptClientStatus::AcceptedClient: {

            // Log success
            break;
          }
          case AcceptClientStatus::FailedHandshake: {
            // Log faliure
            // Close the connection, free the SSL pointer the entire shabang
            break;
          }
          default:
            // No pending connection, continue with normal read
            break;
          }
        }

        else if (p_event_list[i].filter == EVFILT_READ) {
          ClientConnection *connection =
              client_manager_.find_connection(current_fd);

          ReadResult result = handle_read(connection);

          switch (result) {
          case ReadResult::DataReceived: {
            if (connection->status == ConnectionStatus::AwaitingAuth) {
              // parse auth packet, call auth function, verify the client
            } else {
            }
            break;
            // Parse, broadcast, print
          }
          case ReadResult::FatalError: {
            // Error
          }
          case ReadResult::WantRead: {
            // enable read event
          }
          case ReadResult::WantWrite: {
            // enable write event
          }
          }
        }
      }
    }
  }
};
