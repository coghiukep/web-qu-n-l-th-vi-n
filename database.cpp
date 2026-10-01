#include "database.hpp"

static void check(int rc, sqlite3* db, const char* what) {
  if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
    throw std::runtime_error(std::string(what) + ": " + sqlite3_errmsg(db));
}

Database::Database(const std::string& path) {
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    std::string msg = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    throw std::runtime_error("open: " + msg);
  }
  exec("PRAGMA foreign_keys=ON;");
}

Database::~Database() { sqlite3_close(db_); }

void Database::exec(const std::string& sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "unknown error";
    sqlite3_free(err);
    throw std::runtime_error("sql: " + msg);
  }
}

void Database::initSchema() {
  exec(R"sql(
    CREATE TABLE IF NOT EXISTS users(
      id INTEGER PRIMARY KEY, email TEXT UNIQUE NOT NULL, name TEXT NOT NULL,
      salt TEXT NOT NULL, pass_hash TEXT NOT NULL,
      role TEXT NOT NULL CHECK(role IN ('customer','librarian')));
    CREATE TABLE IF NOT EXISTS sessions(
      token TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id));
    CREATE TABLE IF NOT EXISTS books(
      id INTEGER PRIMARY KEY, title TEXT NOT NULL, author TEXT NOT NULL, topic TEXT NOT NULL,
      price INTEGER NOT NULL CHECK(price >= 0),
      copies_available INTEGER NOT NULL CHECK(copies_available >= 0),
      stock_for_sale INTEGER NOT NULL CHECK(stock_for_sale >= 0),
      image_url TEXT NOT NULL DEFAULT '');
    CREATE TABLE IF NOT EXISTS loans(
      id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id),
      book_id INTEGER NOT NULL REFERENCES books(id),
      borrowed_at INTEGER NOT NULL, due_at INTEGER NOT NULL, returned_at INTEGER);
    CREATE TABLE IF NOT EXISTS cart_items(
      user_id INTEGER NOT NULL REFERENCES users(id),
      book_id INTEGER NOT NULL REFERENCES books(id),
      qty INTEGER NOT NULL CHECK(qty > 0), PRIMARY KEY(user_id, book_id));
    CREATE TABLE IF NOT EXISTS orders(
      id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id),
      total INTEGER NOT NULL, created_at INTEGER NOT NULL);
    CREATE TABLE IF NOT EXISTS order_items(
      order_id INTEGER NOT NULL REFERENCES orders(id),
      book_id INTEGER NOT NULL REFERENCES books(id),
      qty INTEGER NOT NULL, price INTEGER NOT NULL);
  )sql");
  // Migrate databases created before book covers existed.
  if (scalar("SELECT COUNT(*) FROM pragma_table_info('books') WHERE name='image_url'", 0) == 0)
    exec("ALTER TABLE books ADD COLUMN image_url TEXT NOT NULL DEFAULT ''");
}

Stmt::Stmt(Database& db, const std::string& sql) : db_(db.raw()) {
  check(sqlite3_prepare_v2(db_, sql.c_str(), -1, &s_, nullptr), db_, "prepare");
}
Stmt::~Stmt() { sqlite3_finalize(s_); }

Stmt& Stmt::bind(int64_t v) {
  check(sqlite3_bind_int64(s_, next_++, v), db_, "bind");
  return *this;
}
Stmt& Stmt::bind(const std::string& v) {
  check(sqlite3_bind_text(s_, next_++, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT), db_, "bind");
  return *this;
}
bool Stmt::step() {
  int rc = sqlite3_step(s_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  throw std::runtime_error(std::string("step: ") + sqlite3_errmsg(db_));
}
int64_t Stmt::i(int col) const { return sqlite3_column_int64(s_, col); }
std::string Stmt::t(int col) const {
  auto p = sqlite3_column_text(s_, col);
  return p ? reinterpret_cast<const char*>(p) : "";
}
