#include "test_util.hpp"

using BorrowTest = LibraryFixture;

TEST_F(BorrowTest, BorrowSetsDueDateAndUsesUpACopy) {
  User u = addUser("an");
  int64_t b = addBook("Sherlock", "Doyle", "Trinh thám", 2);
  Loan l = lib.borrow(u.id, b);
  EXPECT_EQ(l.dueAt, now + Library::kLoanSeconds);
  EXPECT_EQ(lib.listBooks()[0].copiesAvailable, 1);
  EXPECT_EQ(lib.activeLoans(u.id).size(), 1u);
}

TEST_F(BorrowTest, UnknownBook) {
  User u = addUser("an");
  EXPECT_ERR(lib.borrow(u.id, 999), "BOOK_NOT_FOUND");
}

TEST_F(BorrowTest, NoCopiesLeft) {
  User a = addUser("an"), b = addUser("binh");
  int64_t book = addBook("Dune", "Herbert", "Khoa học", 1);
  lib.borrow(a.id, book);
  EXPECT_ERR(lib.borrow(b.id, book), "NO_COPIES_AVAILABLE");
}

TEST_F(BorrowTest, CannotBorrowSameBookTwice) {
  User u = addUser("an");
  int64_t book = addBook("Dune", "Herbert", "Khoa học", 5);
  lib.borrow(u.id, book);
  EXPECT_ERR(lib.borrow(u.id, book), "ALREADY_BORROWED");
}

TEST_F(BorrowTest, LimitOfFiveActiveLoans) {
  User u = addUser("an");
  for (int i = 0; i < Library::kMaxActiveLoans; ++i) lib.borrow(u.id, addBook("Sách " + std::to_string(i)));
  EXPECT_ERR(lib.borrow(u.id, addBook("Sách thứ tư")), "LOAN_LIMIT_REACHED");
}

TEST_F(BorrowTest, ReturningFreesALoanSlotAndACopy) {
  User u = addUser("an");
  int64_t b = addBook("Dune");
  Loan l = lib.borrow(u.id, b);
  EXPECT_EQ(lib.returnBook(u.id, l.id), 0);
  EXPECT_EQ(lib.listBooks()[0].copiesAvailable, 1);
  EXPECT_TRUE(lib.activeLoans(u.id).empty());
  EXPECT_NO_THROW(lib.borrow(u.id, b));  // can borrow it again after returning
}

TEST_F(BorrowTest, ReturnOnDueDateIsFree_OneSecondLateCostsOneDay) {
  User u = addUser("an");
  Loan l1 = lib.borrow(u.id, addBook("A"));
  now = l1.dueAt;
  EXPECT_EQ(lib.returnBook(u.id, l1.id), 0);

  Loan l2 = lib.borrow(u.id, addBook("B"));
  now = l2.dueAt + 1;
  EXPECT_EQ(lib.returnBook(u.id, l2.id), Library::kLateFeePerDay);
}

TEST_F(BorrowTest, LateFeeCountsStartedDays) {
  User u = addUser("an");
  Loan l = lib.borrow(u.id, addBook("A"));
  now = l.dueAt + 2 * 86400 + 1;  // 2 days and 1 second late -> 3 started days
  EXPECT_EQ(lib.returnBook(u.id, l.id), 3 * Library::kLateFeePerDay);
}

TEST_F(BorrowTest, OverdueLoanBlocksNewBorrowing) {
  User u = addUser("an");
  lib.borrow(u.id, addBook("A"));
  advanceDays(15);
  EXPECT_ERR(lib.borrow(u.id, addBook("B")), "HAS_OVERDUE");
  EXPECT_TRUE(lib.activeLoans(u.id)[0].overdue);
}

TEST_F(BorrowTest, CannotReturnSomeoneElsesLoanOrReturnTwice) {
  User a = addUser("an"), b = addUser("binh");
  Loan l = lib.borrow(a.id, addBook("A"));
  EXPECT_ERR(lib.returnBook(b.id, l.id), "LOAN_NOT_FOUND");
  lib.returnBook(a.id, l.id);
  EXPECT_ERR(lib.returnBook(a.id, l.id), "LOAN_NOT_FOUND");
}

TEST_F(BorrowTest, LibrarianSeesAllActiveLoans) {
  User a = addUser("an"), b = addUser("binh");
  lib.borrow(a.id, addBook("A"));
  lib.borrow(b.id, addBook("B"));
  EXPECT_EQ(lib.allActiveLoans().size(), 2u);
}

using CatalogTest = LibraryFixture;

TEST_F(CatalogTest, AddBookValidation) {
  EXPECT_ERR(lib.addBook(Book{0, "", "A", "T", 1, 1, 1}), "INVALID_BOOK");
  EXPECT_ERR(lib.addBook(Book{0, "T", "A", "T", -1, 1, 1}), "INVALID_BOOK");
  EXPECT_ERR(lib.addBook(Book{0, "T", "A", "T", 1, -1, 1}), "INVALID_BOOK");
}

TEST_F(CatalogTest, FilterByTopicAndSearchAndListTopics) {
  addBook("Dune", "Herbert", "Khoa học");
  addBook("Sherlock Holmes", "Doyle", "Trinh thám");
  addBook("Foundation", "Asimov", "Khoa học");
  EXPECT_EQ(lib.listBooks("Khoa học").size(), 2u);
  EXPECT_EQ(lib.listBooks("", "holmes").size(), 1u);   // case-insensitive title search
  EXPECT_EQ(lib.listBooks("", "asimov").size(), 1u);   // author search
  EXPECT_TRUE(lib.listBooks("Không có chủ đề này").empty());
  EXPECT_EQ(lib.topics(), (std::vector<std::string>{"Khoa học", "Trinh thám"}));
}

TEST_F(CatalogTest, SearchTreatsInputAsDataNotSql) {
  addBook("Dune");
  EXPECT_TRUE(lib.listBooks("", "'; DROP TABLE books; --").empty());
  EXPECT_EQ(lib.listBooks().size(), 1u);
}

using CartTest = LibraryFixture;

TEST_F(CartTest, AddAccumulatesQuantityAndComputesTotal) {
  User u = addUser("an");
  int64_t b1 = addBook("A", "x", "t", 1, 5, 30000), b2 = addBook("B", "x", "t", 1, 5, 20000);
  lib.addToCart(u.id, b1, 2);
  lib.addToCart(u.id, b1, 1);
  lib.addToCart(u.id, b2, 1);
  Cart c = lib.cart(u.id);
  EXPECT_EQ(c.items.size(), 2u);
  EXPECT_EQ(c.total, 3 * 30000 + 20000);
}

TEST_F(CartTest, RejectsBadQuantityUnknownBookAndOverStock) {
  User u = addUser("an");
  int64_t b = addBook("A", "x", "t", 1, 3);
  EXPECT_ERR(lib.addToCart(u.id, b, 0), "INVALID_QUANTITY");
  EXPECT_ERR(lib.addToCart(u.id, b, -2), "INVALID_QUANTITY");
  EXPECT_ERR(lib.addToCart(u.id, 999, 1), "BOOK_NOT_FOUND");
  lib.addToCart(u.id, b, 2);
  EXPECT_ERR(lib.addToCart(u.id, b, 2), "OUT_OF_STOCK");  // 2 in cart + 2 > stock 3
}

TEST_F(CartTest, CheckoutDecrementsStockClearsCartAndRecordsOrder) {
  User u = addUser("an");
  int64_t b = addBook("A", "x", "t", 1, 5, 40000);
  lib.addToCart(u.id, b, 2);
  Order o = lib.checkout(u.id);
  EXPECT_EQ(o.total, 80000);
  EXPECT_EQ(lib.listBooks()[0].stockForSale, 3);
  EXPECT_TRUE(lib.cart(u.id).items.empty());
  EXPECT_EQ(db.scalar("SELECT COUNT(*) FROM order_items WHERE order_id=?", 0, o.id), 1);
}

TEST_F(CartTest, EmptyCartCannotCheckout) {
  User u = addUser("an");
  EXPECT_ERR(lib.checkout(u.id), "EMPTY_CART");
}

TEST_F(CartTest, DeclinedPaymentChangesNothing) {
  User u = addUser("an");
  int64_t b = addBook("A", "x", "t", 1, 5);
  lib.addToCart(u.id, b, 2);
  EXPECT_ERR(lib.checkout(u.id, "declined"), "PAYMENT_DECLINED");
  EXPECT_EQ(lib.listBooks()[0].stockForSale, 5);
  EXPECT_EQ(lib.cart(u.id).items.size(), 1u);
  EXPECT_EQ(db.scalar("SELECT COUNT(*) FROM orders", 0), 0);
}

TEST_F(CartTest, StockSoldToSomeoneElseBeforeCheckout) {
  User a = addUser("an"), b = addUser("binh");
  int64_t book = addBook("A", "x", "t", 1, 2);
  lib.addToCart(a.id, book, 2);
  lib.addToCart(b.id, book, 2);
  lib.checkout(a.id);
  EXPECT_ERR(lib.checkout(b.id), "OUT_OF_STOCK");
  EXPECT_EQ(lib.listBooks()[0].stockForSale, 0);
}

TEST_F(CartTest, CartsAreIsolatedPerUser) {
  User a = addUser("an"), b = addUser("binh");
  int64_t book = addBook("A", "x", "t", 1, 5);
  lib.addToCart(a.id, book, 1);
  EXPECT_TRUE(lib.cart(b.id).items.empty());
}

// Crow only knows a fixed set of status codes (e.g. no 402) and answers 500 for any other,
// so every error the service can raise must use one of these.
TEST_F(CartTest, ErrorsUseHttpStatusesTheWebFrameworkSupports) {
  User u = addUser("an");
  addBook("A", "x", "t", 1, 5);
  lib.addToCart(u.id, 1, 1);
  try { lib.checkout(u.id, "declined"); FAIL(); } catch (const LibraryError& e) { EXPECT_EQ(e.http, 400); }
}

TEST_F(CartTest, ClearCartEmptiesCartAndKeepsStock) {
  User u = addUser("an");
  int64_t b = addBook("A", "x", "t", 1, 5);
  lib.addToCart(u.id, b, 2);
  lib.clearCart(u.id);
  EXPECT_TRUE(lib.cart(u.id).items.empty());
  EXPECT_EQ(lib.listBooks()[0].stockForSale, 5);
  EXPECT_EQ(db.scalar("SELECT COUNT(*) FROM orders", 0), 0);
}

TEST_F(CatalogTest, CoverImageIsStoredAndValidated) {
  Book b{0, "T", "A", "T", 1, 1, 1, "https://x.io/a.jpg"};
  lib.addBook(b);
  EXPECT_EQ(lib.listBooks()[0].imageUrl, "https://x.io/a.jpg");
  b.imageUrl = "javascript:alert(1)";
  EXPECT_ERR(lib.addBook(b), "INVALID_IMAGE");
}
