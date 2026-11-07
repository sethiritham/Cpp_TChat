#include "common/protocol.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

enum class ProcessStreamStatus {

  BufferEmpty,
  InvalidProtocolError,
  IncompletePacket,

  PrintMessage,
  ReceivedPong,
  Acknowledgement,
  ErrorAck
};

struct ProcessStreamResult {
  ProcessStreamStatus status;
  std::string message;
  std::string sender;
};

ProcessStreamResult process_data_stream(std::vector<uint8_t> &rx_buffer);
