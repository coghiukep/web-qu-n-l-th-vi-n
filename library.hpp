#pragma once
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "database.hpp"
#include "errors.hpp"

struct Book {
  int64_t id = 0;
  std::string title, author, topic;
  int64_t price = 0;      // VND
  int copiesAvailable = 0;  // copies that can be borrowed
  int stockForSale = 0;     // copies that can be bought
  std::string imageUrl;     // cover: http(s) link or data:image/... (empty = placeholder)
};

struct Loan {  // an active (not yet returned) loan
  int64_t id = 0, userId = 0, bookId = 0;
  std::string title, userName;
  int64_t borrowedAt = 0, dueAt = 0;
  bool overdue = false;
};

struct CartItem { int64_t bookId; std::string title; int64_t price; int qty; };
struct Cart { std::vector<CartItem> items; int64_t total = 0; };
struct Order { int64_t id; int64_t total; int64_t createdAt; };

class Library {
 public:
  using Clock = std::function<int64_t()>;  // injectable so tests control "now"

  static constexpr int kMaxActiveLoans = 5;
  static constexpr int64_t kLoanSeconds = 14 * 86400;
  static constexpr int64_t kLateFeePerDay = 2000;  // VND per started day late

  explicit Library(Database& db, Clock clock = [] { return static_cast<int64_t>(std::time(nullptr)); })
      : db_(db), clock_(std::move(clock)) {}

  // Catalogue
  int64_t addBook(const Book& b);
  std::vector<Book> listBooks(const std::string& topic = "", const std::string& query = "");
  std::vector<std::string> topics();

  // Borrowing
  Loan borrow(int64_t userId, int64_t bookId);
  int64_t returnBook(int64_t userId, int64_t loanId);  // returns late fee (0 if on time)
  std::vector<Loan> activeLoans(int64_t userId);       // one user's loans
  std::vector<Loan> allActiveLoans();                  // librarian view

  // Buying (payment is simulated)
  void addToCart(int64_t userId, int64_t bookId, int qty);
  Cart cart(int64_t userId);
  void clearCart(int64_t userId);  // "hủy đơn": drop everything in the cart
  Order checkout(int64_t userId, const std::string& payment = "card");  // payment "declined" simulates failure

 private:
  std::vector<Loan> queryLoans(int64_t userIdOrZero);
  Database& db_;
  Clock clock_;
};
