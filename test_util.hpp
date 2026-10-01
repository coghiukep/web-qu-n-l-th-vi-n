#pragma once
#include <gtest/gtest.h>

#include "auth.hpp"
#include "library.hpp"
#include "recommender.hpp"

// Asserts that `expr` throws LibraryError with the given machine-readable code.
#define EXPECT_ERR(expr, errcode)                                   \
  do {                                                              \
    try {                                                           \
      expr;                                                         \
      ADD_FAILURE() << "expected LibraryError " << (errcode);       \
    } catch (const LibraryError& e) {                               \
      EXPECT_EQ(e.code, errcode);                                   \
    }                                                               \
  } while (0)

// Fresh in-memory database per test, plus a fake clock the test can move forward.
class LibraryFixture : public ::testing::Test {
 protected:
  void SetUp() override { db.initSchema(); }

  User addUser(const std::string& name) { return auth.registerUser(name + "@test.io", name, "password123"); }
  int64_t addBook(const std::string& title, const std::string& author = "Tác giả A",
                  const std::string& topic = "Trinh thám", int copies = 1, int stock = 1, int64_t price = 50000) {
    return lib.addBook(Book{0, title, author, topic, price, copies, stock});
  }
  void advanceDays(int days) { now += static_cast<int64_t>(days) * 86400; }

  Database db{":memory:"};
  Auth auth{db};
  int64_t now = 1'700'000'000;
  Library lib{db, [this] { return now; }};
};
