#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

struct ClientConnection;

enum class SendPacketStatus { PacketSent, InvalidSSL, FailedToSend };

enum class ReceiveStatus {
  DataReceived,
  WantRead,
  WantWrite,
  ConnectionClosed,
  TransportError,
  TLSFailure,
  InvalidSSL
};

void queue_write(ClientConnection *connection,
                 const std::vector<uint8_t> &data);

std::string read_username(size_t max_size);

std::string read_password(size_t max_size);

SendPacketStatus send_packet(const std::vector<uint8_t> &pkt, SSL *ssl);

ReceiveStatus receive_data(SSL *ssl, std::vector<uint8_t> &rx_buffer);
