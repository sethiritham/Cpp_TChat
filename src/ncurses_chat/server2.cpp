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
      std::cout << "Packet does not contain protocol key" << std::endl;
      close(session.fd);
      g_clients.erase(session.fd);
      return;
    }

    uint32_t payload_len = header.payload_length;
    size_t total_size = sizeof(PacketHeader) + payload_len;

    if (session.rx_buffer.size() < total_size) {
      std::cout << "[INCOMPLETE PACKET]" << std::endl;
      break;
    }

    std::vector<uint8_t> complete_packet(session.rx_buffer.begin(),
                                         session.rx_buffer.end());

    broadcast(session.fd, complete_packet);
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

  bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
  listen(server_fd, SOMAXCONN);

  set_nonblocking(server_fd);
  set_nonblocking(STDIN_FILENO);

  int kq = kqueue();
  struct kevent init_evs[2];
  EV_SET(&init_evs[0], server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  EV_SET(&init_evs[0], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
         nullptr);
  kevent(kq, init_evs, 2, nullptr, 0, nullptr);

  std::cout << "[SERVER] active on PORT: " << PORT << std::endl;
  std::vector<struct kevent> event_list(MAX_EVENTS);

  while (true) {
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
        char buf[256];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);

        if (n > 0) {
          buf[n] = '\0';
          std::cout << "[ADMIN EXEC]: " << buf;
        }

      }

      else if (current_fd == server_fd) {
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd =
            accept(server_fd, (struct sockaddr *)&client_addr, &len);

        if (client_fd >= 0) {
          set_nonblocking(client_fd);
          struct kevent ev;

          EV_SET(&ev, client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
                 nullptr);
          kevent(kq, &ev, 1, nullptr, 0, nullptr);

          g_clients[client_fd] = ClientSession(client_fd);
          g_clients[client_fd].rx_buffer = {};
          g_clients[client_fd].tx_buffer = {};
          g_clients[client_fd].username = "";

          std::cout << "Accepted client fd: " << client_fd << std::endl;
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
          std::cout << "[fd " << current_fd << "] Recieved: " << buffer
                    << std::endl;

          process_client_stream(session);

        } else if (bytes_read == 0 || (bytes_read < 0 && (errno != EAGAIN))) {
          close(current_fd);
          g_clients.erase(current_fd);
          std::cout << "Client disconnected on fd: " << current_fd << std::endl;
        }
      }
    }
  }

  close(kq);
  close(server_fd);
  return 0;
}
