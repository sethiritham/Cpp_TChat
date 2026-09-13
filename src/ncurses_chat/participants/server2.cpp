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
#include <ostream>
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

bool set_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return false;
  return fcntl(fd, F_SETFL, flags | ~O_NONBLOCK) != -1;
}

void broadcast(int sender_fd, const std::vector<uint8_t> &packet) {
  for (auto &[fd, session] : g_clients) {
    if (fd != sender_fd) {
      session.tx_buffer.insert(session.tx_buffer.end(), packet.begin(),
                               packet.end());

      ssize_t sent =
          write(fd, session.tx_buffer.data(), session.tx_buffer.size());

      if (sent > 0) {
        session.tx_buffer.erase(session.tx_buffer.begin(),
                                session.tx_buffer.end());
      }
    }
  }
}

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

    std::string usr_msg = "[" + session.username + "]: " + message;
    safePrint(usr_msg);

    broadcast(session.fd, complete_packet);
  }
}

int handle_client(int fd) {
  set_blocking(fd);
  struct timeval tv{.tv_sec = 3, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  char buffer[512];

  ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

  if (bytes_read < static_cast<ssize_t>(sizeof(PacketHeader))) {
    std::string err_msg = "DID NOT RECIEVE AUTH PACKET\n";
    safePrint(err_msg);
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
      send(fd, packet.data(), packet.size(), 0);
      close(fd);
      return -1;
    }
  } else if (header.type == 0x08) {
    if (!verify_user(name, password)) {
      packet = create_packet_stream(0x08, "NO");
      send(fd, packet.data(), packet.size(), 0);
      close(fd);
      return -1;
    }
  }

  std::string auth_msg = "[AUTH SUCCESS] : " + name + " joined the chat";
  safePrint(auth_msg);

  packet = create_packet_stream(0x08, "OK");
  send(fd, packet.data(), packet.size(), 0);

  g_clients[fd] = ClientSession(fd);
  g_clients[fd].username = name;

  return 0;
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

  setupNcurses();
  safePrint("Server started on port 8080");
  safePrint("Waiting for clients to join");

  set_nonblocking(server_fd);
  set_nonblocking(STDIN_FILENO);

  int kq = kqueue();
  struct kevent init_evs[2];
  EV_SET(&init_evs[0], server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  EV_SET(&init_evs[1], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  kevent(kq, init_evs, 2, nullptr, 0, nullptr);

  safePrint("[SERVER] active on PORT: 8080");
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
        int ch;
        while ((ch = wgetch(inputWin)) != ERR) {
          if (ch == '\n' || ch == KEY_ENTER) {
            if (input_buffer == "/quit") {
              running = false;
              break;
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

      }

      else if (current_fd == server_fd) {
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        int client_fd =
            accept(server_fd, (struct sockaddr *)&client_addr, &len);

        if (client_fd >= 0) {
          int auth_result = handle_client(client_fd);

          if (auth_result == 0) {
            set_nonblocking(client_fd);
            struct kevent ev;

            EV_SET(&ev, client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
                   nullptr);
            kevent(kq, &ev, 1, nullptr, 0, nullptr);

            std::string ack =
                "Accepted client\nNAME: " + g_clients[client_fd].username;
            safePrint(ack.c_str());
          }
        } else {
        }

        continue;
      }

      else if (event_list[i].filter == EVFILT_READ) {
        char buffer[1024];
        ssize_t bytes_read = read(current_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
          auto &session = g_clients[current_fd];
          session.rx_buffer.insert(session.rx_buffer.end(), buffer,
                                   buffer + bytes_read);

          process_client_stream(session);

        } else if (bytes_read == 0 || (bytes_read < 0 && (errno != EAGAIN))) {
          close(current_fd);
          g_clients.erase(current_fd);
          std::string ack = g_clients[current_fd].username + " disconnected!\n";
          safePrint(ack);
        }
      }
    }
  }

  close(kq);
  close(server_fd);
  return 0;
}
