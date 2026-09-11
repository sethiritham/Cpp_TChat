#include <bcrypt.h>
#include <iostream>
#include <sqlite3.h>
#include <string>

constexpr int BCRYPT_WF = 12;

int exec_sql(const std::string &sql_cmnd, sqlite3 *DB) {
  int exit;

  char *error_msg;
  exit = sqlite3_exec(DB, sql_cmnd.c_str(), NULL, 0, &error_msg);

  if (exit != SQLITE_OK) {
    std::cerr << "SQL Error: " << error_msg << std::endl;
    sqlite3_free(error_msg);
    return -1;
  } else {
    std::cout << "COMMAND EXECUTED!" << std::endl;
    return 0;
  }
}

int create_table() {
  sqlite3 *DB;

  int exit = sqlite3_open("user_pass.db", &DB);
  if (exit != SQLITE_OK) {
    std::cout << "UNABLE TO OPEN DATABASE" << std::endl;
    return -1;
  }

  std::string sql = "CREATE TABLE IF NOT EXISTS users ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "username TEXT UNIQUE NOT NULL,"
                    "password_hash TEXT NOT NULL,"
                    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                    ");";

  if (exec_sql(sql, DB) != SQLITE_OK) {
    return -1;
  }

  sqlite3_close(DB);
  return 0;
}

bool add_user(const std::string &username, const std::string &password) {

  sqlite3 *DB;

  int exit = sqlite3_open("user_pass.db", &DB);

  if (exit != SQLITE_OK) {
    std::cout << "UNABLE TO OPEN DATABASE" << std::endl;
    return false;
  }

  std::string hashed_pass = bcrypt::generateHash(password);

  std::string sql = "INSERT INTO users (username) VALUES (?);";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
      sqlite3_int64 new_id = sqlite3_last_insert_rowid(DB);
      std::cout << "Added " << username << " with Auto-ID: " << new_id
                << std::endl;
    } else if (sqlite3_step(stmt) == SQLITE_CONSTRAINT ||
               sqlite3_step(stmt) == SQLITE_CONSTRAINT_UNIQUE) {
      std::cerr << "Error: The name '" << username << "' already exists!"
                << std::endl;
    } else {
      std::cout << "Execution failed" << std::endl;
    }
  }

  sqlite3_finalize(stmt);
  return true;
}

bool verify_user(const std::string &username, const std::string &password) {
  sqlite3 *DB;

  sqlite3_open("user_pass.db", &DB);
  std::string sql =
      "SELECT password_hash from users where username = ? LIMIT 1;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(DB)
              << std::endl;
    return "";
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  std::string result = "";

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *text = sqlite3_column_text(stmt, 0);
    if (text != nullptr) {
      result = reinterpret_cast<const char *>(text);
    }
  } else {
    std::cout << "User not found or column is empty." << std::endl;
    return false;
  }

  sqlite3_finalize(stmt);

  return bcrypt::validatePassword(password, result);
}
