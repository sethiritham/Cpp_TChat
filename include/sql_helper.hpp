#include <bcrypt.h>
#include <cstddef>
#include <sqlite3.h>
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
