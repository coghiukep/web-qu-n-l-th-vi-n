#pragma once
#include <sqlite3.h>

#include <cstdint>
#include <stdexcept>
#include <string>

class Database {
 public:
  explicit Database(const std::string& path);  // ":memory:" for tests
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  sqlite3* raw() const { return db_; }
  void exec(const std::string& sql);
  void initSchema();
  int64_t lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

  // First column of the first row, or `def` when there is no row.
  template <class... A>
  int64_t scalar(const std::string& sql, int64_t def, const A&... args);

 private:
  sqlite3* db_ = nullptr;
};

// Thin RAII wrapper around a prepared statement. bind() fills ?1, ?2, ... in order.
class Stmt {
 public:
  Stmt(Database& db, const std::string& sql);
  ~Stmt();
  Stmt(const Stmt&) = delete;
  Stmt& operator=(const Stmt&) = delete;

  Stmt& bind(int64_t v);
  Stmt& bind(int v) { return bind(static_cast<int64_t>(v)); }
  Stmt& bind(const std::string& v);
  bool step();  // true while a row is available
  void run() { step(); }
  int64_t i(int col) const;
  std::string t(int col) const;

 private:
  sqlite3_stmt* s_ = nullptr;
  sqlite3* db_;
  int next_ = 1;
};

template <class... A>
int64_t Database::scalar(const std::string& sql, int64_t def, const A&... args) {
  Stmt s(*this, sql);
  (s.bind(args), ...);
  return s.step() ? s.i(0) : def;
}

// BEGIN IMMEDIATE ... COMMIT; rolls back automatically if commit() is not reached.
class Tx {
 public:
  explicit Tx(Database& db) : db_(db) { db_.exec("BEGIN IMMEDIATE"); }
  ~Tx() {
    if (!done_) {
      try { db_.exec("ROLLBACK"); } catch (...) {}
    }
  }
  Tx(const Tx&) = delete;
  Tx& operator=(const Tx&) = delete;
  void commit() { db_.exec("COMMIT"); done_ = true; }

 private:
  Database& db_;
  bool done_ = false;
};
