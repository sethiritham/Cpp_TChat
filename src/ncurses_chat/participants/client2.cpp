/**
 * @file client2.cpp
 * @brief Client specific logic
 * client authentication logic, Client main loop
 */

#include "auth.hpp"
#include "chat2.hpp"
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

/**
 * @class ClientSession
 * @brief All the data related to the client, socket, name, input and output
 * buffer
 */
struct ClientSession {
  /**
   * @brief Client Socket
   */
  int fd;
  /**
   * @brief Username of the client
   */
  std::string username;
  /**
   * @brief input buffer, data received from the server is placed here
   */
  std::vector<uint8_t> rx_buffer;
  /**
   * @brief output buffer, data transmitted by the client to the server is
   * placed here
   */
  std::vector<uint8_t> tx_buffer;

  /**
   * @brief Default constructor, does nothing.
   */
  ClientSession() {}

  /**
   * @brief Primary constructor of ClientSession, initializes client socket and
   * reserves both the buffer with size 64KB.
   *
   * @param fd Client Socket (file descriptor)
   */
  ClientSession(int fd) : fd(fd) {
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
 * @brief Handles the authentication logic for the client
 * Handles both registration and login, a packet containing name|password is
 * sent to the server for authentication.
 * @return success: client fd | faliure: -1
 */
int client_login() {
  setupNcurses();
  nodelay(inputWin, true);

  std::string ip;
  printw("Enter server IP (Press enter for localhost 127.0.0.1): ");
  ip = read_input_line(15);

  if (ip.empty())
    ip = "127.0.0.1";

  struct sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr = inet_addr(ip.c_str());

  if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) < 0) {
    perror("INVALID IP");
    return false;
  }

  clear();
  move(0, 0);

  printw("REEGISTER OR LOGIN\n");
  printw("1 - REGISTER\n2 - LOGIN\n");

  refresh();

  int ch;
  ch = getch();

  ClientSession session;

  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  session.fd = client_fd;
  g_clients[client_fd] = session;

  uint8_t pkt_type;

  pkt_type = (ch == '1' || ch == 'r' || ch == 'R') ? 0x09 : 0x08;

  std::string name, pass;

  if (pkt_type == 0x09) {
    printw("------REGISTER------\n");
    refresh();
  } else {
    printw("------LOGIN------\n");
    refresh();
  }

  printw("USERNAME: ");
  refresh();
  name = read_input_line(62);

  printw("\nPASSWORD: ");
  refresh();
  pass = read_password(15);

  g_clients[client_fd].username = name;

  std::string creds = name + "|" + pass;

  if (connect(session.fd, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    perror("Error connecting to server");
    return false;
  }

  auto auth_packet = create_packet_stream(pkt_type, creds);
  send(client_fd, auth_packet.data(), auth_packet.size(), 0);

  clear();
  move(0, 0);
  refresh();

  uint8_t response_buffer[256];
  ssize_t bytes_read =
      read(client_fd, response_buffer, sizeof(response_buffer));

  if (bytes_read < static_cast<ssize_t>(sizeof(PacketHeader))) {
    cleanupNcurses();
    std::cerr << "Authentication failed: Server closed connection or sent no "
                 "response.\n";
    close(client_fd);
    return -1;
  }

  PacketHeader auth_header;

  std::memcpy(&auth_header, response_buffer, sizeof(PacketHeader));
  uint32_t payload_length = ntohl(auth_header.payload_length);

  std::string ack_msg(reinterpret_cast<const char *>(response_buffer) +
                          sizeof(PacketHeader),
                      payload_length);

  if (ack_msg != "OK") {
    cleanupNcurses();
    std::cerr << "Authentication / Registration failed: Server returned '"
              << ack_msg << "'\n";
    close(client_fd);
    return -1;
  }

  clear();
  refresh();
  endwin();

  return client_fd;
}

/**
 * @brief contains the main loop of the client, initializes the ncurses screen,
 * client connects, read & write handling
 * @return
 */
int main() {

  int client_fd = client_login();

  setupNcurses();

  if (client_fd < 0) {
    perror("LOGIN ERROR");
    return -1;
  }

  signal(SIGPIPE, SIG_IGN);

  set_nonblocking(client_fd);
  set_nonblocking(STDIN_FILENO);

  int kq = kqueue();

  struct kevent evs[2];

  EV_SET(&evs[0], client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
  EV_SET(&evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);

  int kevent_error = 0;
  kevent_error = kevent(kq, evs, 2, nullptr, 0, nullptr);

  if (kevent_error < 0) {
    perror("kevent registration failed");
  }

  std::string input_buffer;
  std::vector<uint8_t> rx_buffer;
  std::vector<struct kevent> event_list(MAX_EVENTS);

  bool running = true;

  printf("IS IT FAILING HERE?");

  while (running) {
    int nevents =
        kevent(kq, nullptr, 0, event_list.data(), MAX_EVENTS, nullptr);
    if (nevents < 0) {
      if (errno == EINTR)
        continue;
      break;
    }

    for (int i = 0; i < nevents; ++i) {
      int current_fd = static_cast<int>(event_list[i].ident);

      if (current_fd == STDIN_FILENO) {
        int ch;
        while ((ch = wgetch(inputWin)) != ERR) {
          if (ch == '\n' || ch == KEY_ENTER) {
            if (input_buffer == "/quit") {
              running = false;
              break;
            }

            if (!input_buffer.empty()) {
              safePrint("[YOU] : " + input_buffer);

              input_buffer =
                  "[" + g_clients[client_fd].username + "]: " + input_buffer;

              auto packet = create_packet_stream(0x01, input_buffer);

              write(client_fd, packet.data(), packet.size());

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
      } else if (current_fd == client_fd &&
                 event_list[i].filter == EVFILT_READ) {
        uint8_t buffer[1024];
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

        if (bytes_read > 0) {
          rx_buffer.insert(rx_buffer.end(), buffer, buffer + bytes_read);

          while (rx_buffer.size() >= sizeof(PacketHeader)) {
            PacketHeader header;
            std::memcpy(&header, rx_buffer.data(), sizeof(PacketHeader));
            uint32_t payload_len = ntohl(header.payload_length);
            size_t total_size = payload_len + sizeof(PacketHeader);

            if (rx_buffer.size() < total_size) {
              break;
            }

            std::string msg(rx_buffer.begin() + sizeof(PacketHeader),
                            rx_buffer.begin() + total_size);

            safePrint(msg);

            rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + total_size);
          }
        } else if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN)) {
          safePrint("[SERVER] : CONNECTION LOST");
          running = false;
        }
      }
    }
  }

  cleanupNcurses();
  close(kq);
  close(client_fd);
  return 0;
}
