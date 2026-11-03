#include "authorization.hpp"
#include "common/network.hpp"
#include "common/protocol.hpp"
#include "common/ui.hpp"
#include "connection.hpp"
#include "io.hpp"
#include "message_handler.hpp"
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
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

#define CLIENT_TIMER_IDENT 999
#define MAX_EVENTS 15
#define PORT 8080

enum class SessionState {
  Connected,
  Reconnected,
  Kicked,
  Expired,
  Disconnected
};

enum class DisconnectStatus {
  Disconnected,
  FailedToDisconnect,
  AlreadyDisconnected
};

enum class LoginAction { EnterChat, RetryLogin, Abort };

class ClientApp {
public:
  int listening_fd_ = -1;
  SSL_CTX *ssl_ctx_ = nullptr;

  std::string console_input_buffer_ = "";

private:
  bool p_running = false;
  int p_kq = -1;
  std::vector<struct kevent> p_event_list;

  using Clock = std::chrono::steady_clock;
  sockaddr_in p_server_addr{};

  std::string p_username = "";
  std::string p_token = "";

  ConnectionManager p_manager;
  int p_num_reconnects = 0;

  SessionState p_state = SessionState::Connected;

private:
  SSL_CTX *init_client_ssl_context();

  bool init_serveraddr();

  bool handle_establish_connection_status(EstablishConnectionStatus status);

  DisconnectStatus disconnect();

  LoginAction handle_login_result(LoginResult result);

  bool perform_login_action(LoginAction action);

  void enable_write_event();

  void disable_write_event();

  bool init_chat();

  bool handle_process_stream_result(ProcessStreamResult result,
                                    const std::string &message);

  void handle_read_result(ReadResult result);

  void process_received_data();

  void handle_flush_result(FlushResult result);

  void handle_timer_event();

  void read_event();

  void write_event();

  void handle_console_input_result(ConsoleInputResult result);

  void console_write_event();

public:
  bool initialize();

  void run();

  void end();

public:
  ClientApp();

  ~ClientApp();

  ClientApp(const ClientApp &) = delete;

  ClientApp &operator=(const ClientApp &) = delete;

  ClientApp(ClientApp &&) = delete;

  ClientApp &operator=(ClientApp &&) = delete;
};
