#include "protocol.hpp"
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

/**
 * @brief Processes the receive buffer of the client session and broadcasts it
 * after packetizing it
 *
 * @param session session from whom server received the data
 */
void process_client_stream(ClientConnection *connection);
