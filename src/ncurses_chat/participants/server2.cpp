/**
 * @file server2.cpp
 * @brief Handles the server logic, Client input buffers, verifying client
 */

#include "auth.hpp"
#include "chat2.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <ostream>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

/**
 * @class ClientSession
 * @brief Contains all the parameters related to the client
 */
struct ClientSession {
  /**
   * @brief Client file descriptor
   */
  int fd;

  /**
   * @brief SSL instance
   */
  SSL *ssl = nullptr;

  /**
   * @brief username
   */
  std::string username;
  /**
   * @brief Input buffer, data received from the server is stored here
   */
  std::vector<uint8_t> rx_buffer;
  /**
   * @brief Output buffer, data transmitted to the server is stored here
   */
  std::vector<uint8_t> tx_buffer;

  /**
   * @brief Default constructor, does nothing
   */
  ClientSession() {}

  /**
   * @brief Primary constructor, initializes file descriptor, input and output
   * buffer reserved with size 64KB
   * @param fd Client file descriptor
   */
  ClientSession(int fd, SSL *ssl) : fd(fd), ssl(ssl) {
    rx_buffer.reserve(65536);
    tx_buffer.reserve(65536);
  }
};

/**
 * @brief Dictionary with key being the client socket and value being the
 * session corresponding to that socket
 */
std::map<int, ClientSession> g_clients;

/**
 * @brief Sets a socket to non blocking
 * @param fd File descriptor of the socket to set non blocking
 * @return true if descriptor was successfully set to non blocking, false if
 * system call failed
 */
bool set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

/**
 * @brief Sets a socket to blocking
 * @param fd File descriptor of the socket to set to blocking
 * @return true if descriptor was successfully set to blocking, false if
 * system call failed
 */
bool set_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return false;
  return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) != -1;
}

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

/**
 * @brief Broadcast logic, server sends the packet to all clients connected to
 * it
 *
 * @param sender_fd File descriptor of the participant who is transmitting the
 * data
 * @param packet Packet to be transmitted
 */
void broadcast(int sender_fd, const std::vector<uint8_t> &packet) {
  for (auto &[fd, session] : g_clients) {
    if (fd != sender_fd) {

      session.tx_buffer.insert(session.tx_buffer.end(), packet.begin(),
                               packet.end());

      ssize_t sent = SSL_write(g_clients[fd].ssl, session.tx_buffer.data(),
                               session.tx_buffer.size());

      if (sent > 0) {
        session.tx_buffer.erase(session.tx_buffer.begin(),
                                session.tx_buffer.end());
      }
    }
  }
}

/**
 * @brief Processes the receive buffer of the client session and broadcasts it
 * after packetizing it
 *
 * @param session session from whom server received the data
 */
void process_client_stream(ClientSession &session) {
  while (session.rx_buffer.size() >= sizeof(PacketHeader)) {
    PacketHeader header;

    std::memcpy(&header, session.rx_buffer.data(), sizeof(PacketHeader));

    if (ntohs(header.magic) != PROTOCOL_KEY) {
      std::string err_msg = "Packet does not contain protocol key";
      safePrint(err_msg);
      close(session.fd);
      g_clients.erase(session.fd);
      return;
    }

    uint32_t payload_len = ntohl(header.payload_length);
    size_t total_size = sizeof(PacketHeader) + payload_len;

    if (session.rx_buffer.size() < total_size) {
      std::string err_msg = "[INCOMPLETE PACKET]\n";
      safePrint(err_msg);
      break;
    }

    std::vector<uint8_t> complete_packet(
        session.rx_buffer.begin(), session.rx_buffer.begin() + total_size);

    std::string message(session.rx_buffer.begin() + sizeof(PacketHeader),
                        session.rx_buffer.begin() + total_size);

    session.rx_buffer.erase(session.rx_buffer.begin(),
                            session.rx_buffer.begin() + total_size);

    std::string usr_msg = message;
    safePrint(usr_msg);

    broadcast(session.fd, complete_packet);
  }
}

/**
 * @brief Client authentication is handled here, after authentication, client
 * session is initialized
 * @param fd File descriptor of the client
 * @return 0 if successfull, -1 for faliure
 */
int handle_client(int fd, SSL_CTX *ssl_ctx) {
  set_blocking(fd);

  SSL *ssl = SSL_new(ssl_ctx);
  SSL_set_fd(ssl, fd);

  if (SSL_accept(ssl) <= 0) {
    std::cerr << "[TLS ERROR] Handshake failed on fd " << fd << std::endl;
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    close(fd);
    return -1;
  }

  g_clients[fd] = ClientSession(fd, ssl);

  safePrint("[TLS SUCCESS] Encrypted connection established on fd ");

  struct timeval tv{.tv_sec = 3, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  char buffer[512];

  ssize_t bytes_read = SSL_read(ssl, buffer, sizeof(buffer));

  if (bytes_read < static_cast<int>(sizeof(PacketHeader))) {
    std::cerr << "[AUTH ERROR] Invalid packet over TLS stream." << std::endl;
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
    return -1;
  }

  PacketHeader header;

  std::memcpy(&header, buffer, sizeof(PacketHeader));

  if (ntohs(header.magic) != PROTOCOL_KEY || ntohl(header.payload_length) < 0) {
    std::string err_msg =
        "INVALID AUTH PACKET (PROTOCOL KEY MISSING OR PACKET EMPTY)";
    safePrint(err_msg);
    return -1;
  }

  char payload[512];

  int payload_length = ntohl(header.payload_length);

  std::memcpy(payload, buffer + sizeof(PacketHeader), payload_length);

  payload[ntohl(header.payload_length)] = '\0';

  std::string message(buffer + sizeof(PacketHeader), payload_length);

  if (message.find("|") == std::string::npos) {
    close(fd);
    return -1;
  }

  std::string name = message.substr(0, message.find("|"));
  std::string password = message.substr(message.find("|") + 1);

  std::vector<uint8_t> packet;

  if (header.type == 0x09) {
    if (!(register_client(name, password))) {
      packet = create_packet_stream(0x09, "NO");
      SSL_write(ssl, packet.data(), packet.size());

      SSL_shutdown(ssl);
      SSL_free(ssl);
      close(fd);
      return -1;
    }
  } else if (header.type == 0x08) {
    if (!verify_user(name, password)) {
      packet = create_packet_stream(0x08, "NO");
      SSL_write(ssl, packet.data(), packet.size());

      SSL_shutdown(ssl);
      SSL_free(ssl);
      close(fd);
      return -1;
    }
  }

  std::string auth_msg = "[AUTH SUCCESS] : " + name + " joined the chat";
  safePrint(auth_msg);

  packet = create_packet_stream(0x08, "OK");
  SSL_write(ssl, packet.data(), packet.size());

  g_clients[fd].username = name;

  auto broad_pack = create_packet_stream(0x04, auth_msg);
  broadcast(fd, broad_pack);

  return 0;
}

int kick_client(const std::string &username) {
  for (auto &[fd, session] : g_clients) {
    if (session.username == username) {

      std::string kick_msg = "[SERVER] : kicked " + username;

      auto kick_pkt = create_packet_stream(0x04, kick_msg);

      broadcast(session.fd, kick_pkt);
      safePrint("[SERVER]: Successfully kicked " + username, true);

      SSL_shutdown(session.ssl);
      SSL_free(session.ssl);
      close(session.fd);
      return 1;
    }
  }

  safePrint("{ERROR} : Client not found!");
  return 0;
}

void handle_read(int current_fd) {

  SSL *ssl = g_clients[current_fd].ssl;
  char buffer[1024];
  ssize_t bytes_read = SSL_read(ssl, buffer, sizeof(buffer));

  if (bytes_read > 0) {
    auto &session = g_clients[current_fd];
    session.rx_buffer.insert(session.rx_buffer.end(), buffer,
                             buffer + bytes_read);

    process_client_stream(session);

  } else if (bytes_read == 0 || (bytes_read < 0 && (errno != EAGAIN))) {
    std::string ack = g_clients[current_fd].username + " disconnected!";

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(current_fd);
    g_clients.erase(current_fd);

    safePrint(ack);
  }
}

bool handle_write(std::string &input_buffer, int server_fd) {
  int ch;
  while ((ch = wgetch(inputWin)) != ERR) {
    if (ch == '\n' || ch == KEY_ENTER) {
      if (input_buffer == "/quit") {
        return false;
      }

      if (input_buffer.find("/kick") != std::string::npos) {
        size_t split_pos = input_buffer.find(" ");

        std::string client_username = input_buffer.substr(split_pos + 1);

        kick_client(client_username);

        input_buffer.clear();

        werase(inputWin);
        mvwprintw(inputWin, 0, 0, "%s", input_buffer.c_str());
        wrefresh(inputWin);

        continue;
      }

      if (!input_buffer.empty()) {
        std::string msg = "[YOU]: " + input_buffer;
        input_buffer = "[SERVER]: " + input_buffer;
        auto packet = create_packet_stream(0x01, input_buffer);

        safePrint(msg);
        broadcast(server_fd, packet);

        input_buffer.clear();
        werase(inputWin);
        wrefresh(inputWin);
      }
    } else if (ch == KEY_BACKSPACE || ch == 127) {
      if (!input_buffer.empty())
        input_buffer.pop_back();
    } else if (isprint(ch)) {
      input_buffer.push_back(static_cast<char>(ch));
    }

    werase(inputWin);
    mvwprintw(inputWin, 0, 0, "%s", input_buffer.c_str());
    wrefresh(inputWin);
  }

  return true;
}

void handle_client_connection(int server_fd, SSL_CTX *ssl_ctx, int kq) {
  sockaddr_in client_addr;
  socklen_t len = sizeof(client_addr);

  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);

  if (client_fd >= 0) {
    int auth_result = handle_client(client_fd, ssl_ctx);

    if (auth_result == 0) {
      set_nonblocking(client_fd);
      struct kevent ev;

      EV_SET(&ev, client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
      kevent(kq, &ev, 1, nullptr, 0, nullptr);
    }
  } else {
  }
}

int main() {
  signal(SIGPIPE, SIG_IGN);

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;

  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;

  addr.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return -1;
  }

  listen(server_fd, SOMAXCONN);

  SSL_CTX *ssl_ctx = init_server_ssl_context();

  setupNcurses();
  safePrint("Waiting for clients to join", true);

  set_nonblocking(server_fd);
  set_nonblocking(STDIN_FILENO);

  int kq = kqueue();
  struct kevent init_evs[2];
  EV_SET(&init_evs[0], server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  EV_SET(&init_evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  kevent(kq, init_evs, 2, nullptr, 0, nullptr);

  safePrint("[SERVER] active on PORT: 8080", true);
  std::vector<struct kevent> event_list(MAX_EVENTS);

  if (!create_table()) {
    return -1;
  }

  std::string input_buffer;
  bool running = true;

  while (running) {
    int nevents =
        kevent(kq, nullptr, 0, event_list.data(), MAX_EVENTS, nullptr);
    if (nevents < 0) {
      if (errno == EINTR)
        continue; // call interupted by async signal
      break;
    }

    for (int i = 0; i < nevents; ++i) {
      int current_fd = static_cast<int>(event_list[i].ident);

      if (current_fd == STDIN_FILENO) {
        if (!handle_write(input_buffer, server_fd))
          running = false;
      }

      else if (current_fd == server_fd) {
        handle_client_connection(server_fd, ssl_ctx, kq);
      }

      else if (event_list[i].filter == EVFILT_READ) {
        handle_read(current_fd);
      }
    }
  }

  close(kq);
  close(server_fd);
  return 0;
}
