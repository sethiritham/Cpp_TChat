#include "authorization.hpp"
#include "common/network.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include "connection.hpp"
#include "server/io.hpp"
#include "server/message_handler.hpp"
#include "server/sql_helper.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#define SERVER_TIMER_IDENT 999

class ServerApp {
public:
  int listening_fd_;
  SSL_CTX *ssl_ctx_;

  ClientManager client_manager_;
  std::string console_input_buffer_;

private:
  bool p_running;
  int p_kq;
  std::vector<struct kevent> p_event_list;

private:
  SSL_CTX *init_server_ssl_context();

  void handle_login_case(VerifyClientStatus status,
                         ClientConnection *connection);

  void handle_register_client_status(RegisterClientStatus status,
                                     ClientConnection *connection);

  void handle_handshake_client(HandshakeResult result,
                               ClientConnection *connection);
  void handle_accept_client_status(AcceptClientStatus status);

  void handle_handshake_stream_status(HandshakeResult handshake_result,
                                      ClientConnection *connection);
  void handle_process_stream_result(const ProcessStreamResult &result,
                                    ClientSession *session,
                                    std::string message);

  void handle_read_result(ClientConnection *connection, ReadResult result);

  void read_event(ClientConnection *connection);

  void write_event(ClientConnection *connection);
  void console_write_event();

  void handle_kick_status(KickStatus status, const std::string &username);

  void handle_console_input_result(ConsoleInputResult result);

  void handle_flush_result(FlushResult result, ClientConnection *connection);

  void client_connection_event();

  void timer_event();

public:
  int initialize();

  void run();

  void end();
};
