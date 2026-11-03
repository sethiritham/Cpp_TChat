#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
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

enum class ProcessStreamResult {
  BufferEmpty,
  IncompletePacket,
  InvalidProtocolError,

  PrintMessage,
  ReceivedPong,
  Acknowledgement,
  ErrorAck
};

ProcessStreamResult process_data_stream(std::vector<uint8_t> &rx_buffer,
                                        std::string &message);
