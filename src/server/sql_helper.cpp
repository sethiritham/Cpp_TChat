#include "server/sql_helper.hpp"
#include <filesystem>
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

bool create_user_database() {

  std::error_code ec;

  std::filesystem::create_directories("database", ec);

  if (ec) {
    std::fprintf(stderr, "Database directory error: %s\n",
                 ec.message().c_str());
    return false;
  }

  sqlite3 *DB = nullptr;

  int result = sqlite3_open(USER_DATABASE_PATH, &DB);

  if (result != SQLITE_OK) {
    std::fprintf(stderr, "Database open failed: %s\n",
                 DB ? sqlite3_errmsg(DB) : "Unable to allocate database");

    if (DB)
      sqlite3_close(DB);

    return false;
  }

  const char *sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL
        );
    )";

  char *error_message = nullptr;

  result = sqlite3_exec(DB, sql, nullptr, nullptr, &error_message);

  if (result != SQLITE_OK) {

    std::fprintf(stderr, "Database initialization failed: %s\n",
                 error_message ? error_message : sqlite3_errmsg(DB));

    sqlite3_free(error_message);
    sqlite3_close(DB);

    return false;
  }

  sqlite3_close(DB);

  return true;
}
