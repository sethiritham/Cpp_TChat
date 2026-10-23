#include "authorization.hpp"
#include "network.hpp"
#include "server_connection.hpp"
#include "server_io.hpp"
#include "sql_helper.hpp"
#include "ui.hpp"
#include <iostream>
#include <string>

VerifyClientStatus verify_client(const std::string &username,
                                 const std::string &password) {
  sqlite3 *DB;

  sqlite3_open("user_pass.db", &DB);
  std::string sql =
      "SELECT password_hash from users where username = ? LIMIT 1;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return VerifyClientStatus::ExecFailed;
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  std::string result = "";

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *text = sqlite3_column_text(stmt, 0);
    if (text != nullptr) {
      result = reinterpret_cast<const char *>(text);
    }
  } else {
    sqlite3_close(DB);
    return VerifyClientStatus::UserNotFound;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(DB);

  return (bcrypt::validatePassword(password, result))
             ? VerifyClientStatus::Authorized
             : VerifyClientStatus::NotAuthorized;
}

bool verify_session(const std::string &username, const std::string &token,
                    const std::map<std::string, ClientSession> &sessions_) {
  for (auto &[name, session] : sessions_) {
    if (name == username && session.token == token) {
      return true;
    }
  }

  return false;
}

RegisterClientStatus add_client(const std::string &username,
                                const std::string &password) {

  sqlite3 *DB;

  int exit = sqlite3_open("user_pass.db", &DB);

  if (exit != SQLITE_OK) {
    sqlite3_close(DB);
    return RegisterClientStatus::UnableToOpenDatabase;
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

      sqlite3_finalize(stmt);
      sqlite3_close(DB);
      return RegisterClientStatus::ClientRegistered;
    } else if (step_res == SQLITE_CONSTRAINT ||
               step_res == SQLITE_CONSTRAINT_UNIQUE) {

      sqlite3_finalize(stmt);
      sqlite3_close(DB);
      return RegisterClientStatus::UsernameExists;
      success = false;
    } else {
      sqlite3_finalize(stmt);
      sqlite3_close(DB);
      return RegisterClientStatus::ExecFailed;
    }
  }
  return RegisterClientStatus::UnknownError;
}
