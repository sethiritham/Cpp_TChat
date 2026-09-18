#include "chat2.hpp"
#include <bcrypt.h>
#include <cstddef>
#include <iostream>
#include <sqlite3.h>
#include <string>

constexpr int BCRYPT_WF = 12;

/**
 * @brief executes a given SQL command query
 * @param sql_cmnd SQL query
 * @param DB pointer to the database
 * @return exit code, 0 for success, -1 for failure
 */
int exec_sql(const std::string &sql_cmnd, sqlite3 *DB);

/**
 * @brief Create the user_pass table
 * @return true for success, false for faliure
 */
bool create_table();

/**
 * @brief Adds username and adds password after hashing it
 * @param username Username
 * @param password password (Unhashed)
 * @return true for success, false for failure
 */
bool add_user(const std::string &username, const std::string &password);

/**
 * @brief Verifies the user
 * @param username Username
 * @param password unhashed password
 * @return true if verified, false if not
 */
bool verify_user(const std::string &username, const std::string &password);

/**
 * @brief reads password and displayes it hidden
 * @param max_len Maximum permitted length of the password
 */
std::string read_password(size_t max_len);

/**
 * @brief reads username and displayes it on the input screen
 * @param max_len Maximum permitted length of the username
 */
std::string read_input_line(size_t max_len);

/**
 * @brief Adds the client to the database
 * @param name Username
 * @param pass password
 * @return true if success, false if failure
 */
bool register_client(const std::string &name, const std::string &pass);
