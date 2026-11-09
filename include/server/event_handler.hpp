#include "authorization.hpp"
#include "common/network.hpp"
#include "manager.hpp"
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
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @class EventHandler
 * @brief Deals with all the registered event for the server
 * Connection, Timer, Read, Write, Console
 */
class EventHandler {

public:
  ClientManager client_manager_;
  std::string console_input_buffer_;

private:
  int p_kq;

private:
  /**
   * @brief Deals with client login for the server side
   *
   * @param status returned by verify_client
   * @param connection Client Connection
   * @param username Client username
   */
  void handle_login_case(VerifyClientStatus status,
                         ClientConnection *connection, std::string username);

  /**
   * @brief Deals with client registration for the server side
   * @param status returned by register_client
   * @param connection Client Connection
   */
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

  void handle_kick_status(KickStatus status, const std::string &username);

  bool handle_console_input_result(ConsoleInputResult result);

  void handle_flush_result(FlushResult result, ClientConnection *connection);

  void enable_write_event(int kq, int fd);

  void disable_write_event(int kq, int fd);

public:
  void set_kq(int kq);
  void read_event(ClientConnection *connection);
  void write_event(ClientConnection *connection);
  bool console_write_event();
  void client_connection_event(int listening_fd, SSL_CTX *ssl_ctx);
  void timer_event();
};
