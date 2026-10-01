#pragma once
#include <optional>
#include <string>

#include "database.hpp"
#include "errors.hpp"

struct User {
  int64_t id = 0;
  std::string email, name, role;  // role: "customer" | "librarian"
};

// SHA-256 as lowercase hex (exposed so tests can check it against known vectors).
std::string sha256hex(const std::string& msg);

class Auth {
 public:
  explicit Auth(Database& db) : db_(db) {}

  // Public sign-up: ALWAYS creates a customer, never a librarian.
  User registerUser(const std::string& email, const std::string& name, const std::string& password);
  // Internal use only (seeding). Not reachable from any HTTP route.
  User createUser(const std::string& email, const std::string& name, const std::string& password,
                  const std::string& role);

  std::string login(const std::string& email, const std::string& password);  // returns session token
  std::optional<User> userFromToken(const std::string& token);
  void logout(const std::string& token);

 private:
  Database& db_;
};
