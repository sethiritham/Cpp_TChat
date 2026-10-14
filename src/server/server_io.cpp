#include "chat2.hpp"
#include "server_app.hpp"
#include "server_connection.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <ostream>
#include <random>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

void queue_write(int fd, const std::vector<uint8_t> &data,
                 ClientManager &manager) {
  ClientConnection *connection = manager.find_connection(fd);

  connection->tx_queue.insert(connection->tx_queue.end(), data.begin(),
                              data.end());
}

void flush_write(ClientConnection *connection) {
  while (!connection->tx_queue.empty()) {
    std::vector<uint8_t> packet = connection->tx_queue.front();

    SSL *ssl = connection->ssl;
    int result = SSL_write(ssl, packet.data(), packet.size());

    if (result > 0) {
      connection->tx_offset += packet.size();
    }
  }
}

void broadcast(int sender_fd, const std::vector<uint8_t> &packet,
               ClientManager &manager) {
  manager.for_each_connection([&](int fd, ClientConnection &connection) {
    if (fd == sender_fd) {
      return;
    }

    queue_write(fd, packet, manager);
  });
}

std::string generate_token() {
  const std::string characters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

  int length = 8;

  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<> distribution(0, characters.size() - 1);

  std::string random_string;
  for (size_t i = 0; i < length; ++i) {
    random_string += characters[distribution(generator)];
  }

  return random_string;
}

/**
 * @brief Processes the receive buffer of the client session and broadcasts it
 * after packetizing it
 *
 * @param session session from whom server received the data
 */
void process_client_stream(ClientSession &session) {

  session.last_seen = time(nullptr);

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

    if (header.type == 0x07) {
      safePrint("Received PING");
      auto pong_packet = create_packet_stream(0x06, "");
      SSL_write(session.ssl, pong_packet.data(), pong_packet.size());

      session.rx_buffer.erase(session.rx_buffer.begin(),
                              session.rx_buffer.begin() + total_size);

      continue;
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
