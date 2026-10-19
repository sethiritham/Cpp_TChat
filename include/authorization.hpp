#include <bcrypt.h>
#include <cstddef>
#include <sqlite3.h>
#include <streambuf>
#include <string>

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
int add_user(const std::string &username, const std::string &password);

/**
 * @brief Verifies the user
 * @param username Username
 * @param password unhashed password
 * @return true if verified, false if not
 */
bool verify_user(const std::string &username, const std::string &password);

bool register_client(const std::string &name, const std::string &pass);
