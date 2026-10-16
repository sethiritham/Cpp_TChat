#include "protocol.hpp"

#include "server_connection.hpp"
#include "ui.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

enum class ProcessStreamResult { BufferEmpty, InvalidProtocolError };
/**
 * @brief Processes the receive buffer of the client session and broadcasts it
 * after packetizing it
 *
 * @param session session from whom server received the data
 */
ProcessStreamResult process_client_stream(ClientSession *session,
                                          ClientManager &manager);
