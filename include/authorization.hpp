#include <bcrypt.h>
#include <cstddef>
#include <string>

enum class RegisterClientStatus {
  UnableToOpenDatabase,
  UsernameExists,
  ExecFailed,
  ClientRegistered,
  UnknownError
};

bool verify_client(const std::string &username, const std::string &password);

RegisterClientStatus register_client(const std::string &name,
                                     const std::string &pass);

bool verify_client_session(const std::string &username,
                           const std::string &token);
