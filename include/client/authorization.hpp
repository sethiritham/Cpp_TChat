#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

enum class LoginStatus {
  Success,
  AuthenticationRejected,
  ConnectionLost,
  ProtocolError
};
struct LoginResult {
  LoginStatus status;
  std::string username;
  std::string token;
};

struct ClientConnection;

LoginResult login_client(ClientConnection *connection);
