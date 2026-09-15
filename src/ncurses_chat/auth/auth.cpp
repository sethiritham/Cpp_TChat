#include "auth.hpp"

int exec_sql(const std::string &sql_cmnd, sqlite3 *DB) {
  int exit;

  char *error_msg;
  exit = sqlite3_exec(DB, sql_cmnd.c_str(), NULL, 0, &error_msg);

  if (exit != SQLITE_OK) {
    std::cerr << "SQL Error: " << error_msg << std::endl;
    sqlite3_free(error_msg);
    return -1;
  } else {
    return 0;
  }
}

bool create_table() {
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
    return false;
  }

  sqlite3_close(DB);
  return true;
}

bool add_user(const std::string &username, const std::string &password) {

  sqlite3 *DB;

  int exit = sqlite3_open("user_pass.db", &DB);

  if (exit != SQLITE_OK) {
    std::string error_msg = "UNABLE TO OPEN DATABASE";
    safePrint(error_msg);
    return false;
  }

  std::string hashed_pass = bcrypt::generateHash(password);

  std::string sql =
      "INSERT INTO users (username, password_hash) VALUES (?, ?);";
  sqlite3_stmt *stmt;
  bool success = false;

  if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashed_pass.c_str(), -1, SQLITE_TRANSIENT);

    int step_res = sqlite3_step(stmt);

    if (step_res == SQLITE_DONE) {
      sqlite3_int64 new_id = sqlite3_last_insert_rowid(DB);
      std::string ack_msg = "Added " + username;
      safePrint(ack_msg);
      success = true;
    } else if (step_res == SQLITE_CONSTRAINT ||
               step_res == SQLITE_CONSTRAINT_UNIQUE) {
      std::string error_msg =
          "Error: The name " + username + " already exists!";
      safePrint(error_msg);
      success = false;
    } else {
      std::string error_msg = "Execution failed";
      safePrint(error_msg);
      success = false;
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(DB);
  return success;
}

bool verify_user(const std::string &username, const std::string &password) {
  sqlite3 *DB;

  sqlite3_open("user_pass.db", &DB);
  std::string sql =
      "SELECT password_hash from users where username = ? LIMIT 1;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    std::string error_msg =
        "Failed to prepare statement: " + std::string(sqlite3_errmsg(DB));
    safePrint(error_msg);
    return false;
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  std::string result = "";

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *text = sqlite3_column_text(stmt, 0);
    if (text != nullptr) {
      result = reinterpret_cast<const char *>(text);
    }
  } else {
    std::string error_msg = "User not found or column is empty.\n";
    safePrint(error_msg);
    sqlite3_close(DB);
    return false;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(DB);

  return bcrypt::validatePassword(password, result);
}

std::string read_input_line(size_t max_len) {
  echo();
  std::string buffer(max_len, '\0');
  getnstr(&buffer[0], max_len - 1);
  buffer.resize(std::strlen(buffer.c_str()));
  return buffer;
}

bool register_client(const std::string &name, const std::string &pass) {
  if (!add_user(name, pass)) {
    std::string error_msg = "COULD NOT ADD USER TO THE DATABASE";
    safePrint(error_msg);
    refresh();
    return false;
  }

  std::string ack_msg = "CLIENT : " + name + "REGISTERED";
  safePrint(ack_msg);

  return true;
}
