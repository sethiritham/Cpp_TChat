#include "chat2.hpp"
#include <bcrypt.h>
#include <cstddef>
#include <iostream>
#include <sqlite3.h>
#include <string>

constexpr int BCRYPT_WF = 12;

int exec_sql(const std::string &sql_cmnd, sqlite3 *DB);

bool create_table();

bool add_user(const std::string &username, const std::string &password);

bool verify_user(const std::string &username, const std::string &password);

std::string read_password(size_t max_len);

std::string read_input_line(size_t max_len);

bool register_client(const std::string &name, const std::string &pass);
