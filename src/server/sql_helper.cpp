#include "sql_helper.hpp"
#include "ui.hpp"
#include <iostream>

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

  int exit = sqlite3_open("database/user_pass.db", &DB);
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
