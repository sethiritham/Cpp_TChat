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
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

struct ClientSession {
  int fd;
  std::string username;
  std::vector<uint8_t> rx_buffer;
  std::vector<uint8_t> tx_buffer;

  ClientSession() {}

  ClientSession(int fd) : fd(fd) {
    rx_buffer.reserve(65536);
    tx_buffer.reserve(65536);
  }
};

std::map<int, ClientSession> g_clients;

bool set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

int client_login() {

  ClientSession session;

  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  session.fd = client_fd;
  g_clients[client_fd] = session;

  std::string ip;
  std::cout << "Enter server IP (Press enter for localhost 127.0.0.1): ";
  std::getline(std::cin, ip);
  if (ip.empty())
    ip = "127.0.0.1";

  struct sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr = inet_addr(ip.c_str());

  if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) < 0) {
    std::cerr << "Invalid IP address" << std::endl;
    return false;
  }

  if (connect(session.fd, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    std::cerr << "Error connecting to server" << std::endl;
    return false;
  }

  std::string name, pass;
  std::cout << "Enter your name: ";
  std::getline(std::cin, name);
  std::cout << "Enter your password: ";
  std::getline(std::cin, pass);
  std::string creds = name + "|" + pass;

  std::vector<uint8_t> lgn_packet = create_packet_stream(0x08, creds);

  write(client_fd, lgn_packet.data(), lgn_packet.size());

  session.username = name;
  // Hash and Store password

  // uint8_t buffer[1024];
  // ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
  //
  // if(!(bytes_read > 0))
  // {
  //   perror("[SERVER] : DID NOT RECEIVE LOGIN PACKET\n");
  //   return -1;
  // }
  //
  // PacketHeader header;
  // std::memcpy(&header, buffer, sizeof(PacketHeader));
  //
  // if(header.type != 0x08)
  // {
  //   std::cout<< "[SERVER] : DID NOT RECEIVE A VALID LOGIN PACKET (TYPE
  //   WRONG)\n"; return -1;
  // }
  //
  // if (std::string(buffer) != "OK") {
  //   std::cerr << "Invalid credentials" << std::endl;
  //   close(session.fd);
  //   return false;
  // }

  return client_fd;
}

int main() {

  int client_fd = client_login();

  if (client_fd < 0) {
    perror("LOGIN ERROR");
    return -1;
  }

  signal(SIGPIPE, SIG_IGN);

  set_nonblocking(client_fd);
  set_nonblocking(STDIN_FILENO);

  setupNcurses();
  nodelay(inputWin, true);

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
            uint32_t payload_len = header.payload_length;
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
