#include "library.hpp"

int64_t Library::addBook(const Book& b) {
  if (b.title.empty() || b.author.empty() || b.topic.empty() || b.price < 0 || b.copiesAvailable < 0 ||
      b.stockForSale < 0)
    throw LibraryError("INVALID_BOOK", "Thông tin sách không hợp lệ", 400);
  const std::string& im = b.imageUrl;
  if (!im.empty() && (im.size() > 300000 || !(im.rfind("https://", 0) == 0 || im.rfind("http://", 0) == 0 ||
                                              im.rfind("data:image/", 0) == 0)))
    throw LibraryError("INVALID_IMAGE", "Ảnh bìa không hợp lệ (link http/https hoặc ảnh tải lên, tối đa ~300KB)", 400);
  Stmt s(db_, "INSERT INTO books(title,author,topic,price,copies_available,stock_for_sale,image_url) VALUES(?,?,?,?,?,?,?)");
  s.bind(b.title).bind(b.author).bind(b.topic).bind(b.price).bind(b.copiesAvailable).bind(b.stockForSale).bind(b.imageUrl);
  s.run();
  return db_.lastInsertId();
}

std::vector<Book> Library::listBooks(const std::string& topic, const std::string& query) {
  Stmt s(db_,
         "SELECT id,title,author,topic,price,copies_available,stock_for_sale,image_url FROM books "
         "WHERE (?='' OR topic=?) AND (?='' OR title LIKE '%'||?||'%' OR author LIKE '%'||?||'%') "
         "ORDER BY title");
  s.bind(topic).bind(topic).bind(query).bind(query).bind(query);
  std::vector<Book> out;
  while (s.step())
    out.push_back({s.i(0), s.t(1), s.t(2), s.t(3), s.i(4), static_cast<int>(s.i(5)), static_cast<int>(s.i(6)), s.t(7)});
  return out;
}

std::vector<std::string> Library::topics() {
  Stmt s(db_, "SELECT DISTINCT topic FROM books ORDER BY topic");
  std::vector<std::string> out;
  while (s.step()) out.push_back(s.t(0));
  return out;
}

Loan Library::borrow(int64_t userId, int64_t bookId) {
  Tx tx(db_);
  const int64_t now = clock_();
  // Checks run in this order; tests rely on it.
  const int64_t avail = db_.scalar("SELECT copies_available FROM books WHERE id=?", -1, bookId);
  if (avail < 0) throw LibraryError("BOOK_NOT_FOUND", "Không tìm thấy sách", 404);
  if (db_.scalar("SELECT COUNT(*) FROM loans WHERE user_id=? AND book_id=? AND returned_at IS NULL", 0, userId,
                 bookId) > 0)
    throw LibraryError("ALREADY_BORROWED", "Bạn đang mượn cuốn này rồi", 409);
  if (avail == 0) throw LibraryError("NO_COPIES_AVAILABLE", "Sách đã hết bản để mượn", 409);
  if (db_.scalar("SELECT COUNT(*) FROM loans WHERE user_id=? AND returned_at IS NULL", 0, userId) >=
      kMaxActiveLoans)
    throw LibraryError("LOAN_LIMIT_REACHED", "Đã đạt giới hạn số sách đang mượn", 409);
  if (db_.scalar("SELECT COUNT(*) FROM loans WHERE user_id=? AND returned_at IS NULL AND due_at<?", 0, userId,
                 now) > 0)
    throw LibraryError("HAS_OVERDUE", "Bạn có sách quá hạn, hãy trả trước khi mượn thêm", 409);

  const int64_t due = now + kLoanSeconds;
  {
    Stmt s(db_, "INSERT INTO loans(user_id,book_id,borrowed_at,due_at) VALUES(?,?,?,?)");
    s.bind(userId).bind(bookId).bind(now).bind(due);
    s.run();
  }
  const int64_t loanId = db_.lastInsertId();
  {
    Stmt s(db_, "UPDATE books SET copies_available=copies_available-1 WHERE id=?");
    s.bind(bookId);
    s.run();
  }
  tx.commit();
  Loan l;
  l.id = loanId; l.userId = userId; l.bookId = bookId; l.borrowedAt = now; l.dueAt = due;
  return l;
}

int64_t Library::returnBook(int64_t userId, int64_t loanId) {
  Tx tx(db_);
  int64_t bookId = 0, due = 0;
  {
    Stmt s(db_, "SELECT book_id,due_at FROM loans WHERE id=? AND user_id=? AND returned_at IS NULL");
    s.bind(loanId).bind(userId);
    if (!s.step()) throw LibraryError("LOAN_NOT_FOUND", "Không tìm thấy lượt mượn đang hoạt động", 404);
    bookId = s.i(0);
    due = s.i(1);
  }
  const int64_t now = clock_();
  {
    Stmt s(db_, "UPDATE loans SET returned_at=? WHERE id=?");
    s.bind(now).bind(loanId);
    s.run();
  }
  {
    Stmt s(db_, "UPDATE books SET copies_available=copies_available+1 WHERE id=?");
    s.bind(bookId);
    s.run();
  }
  tx.commit();
  return now > due ? ((now - due + 86399) / 86400) * kLateFeePerDay : 0;
}

std::vector<Loan> Library::queryLoans(int64_t userIdOrZero) {
  const int64_t now = clock_();
  Stmt s(db_,
         "SELECT l.id,l.user_id,l.book_id,b.title,u.name,l.borrowed_at,l.due_at FROM loans l "
         "JOIN books b ON b.id=l.book_id JOIN users u ON u.id=l.user_id "
         "WHERE l.returned_at IS NULL AND (?=0 OR l.user_id=?) ORDER BY l.due_at");
  s.bind(userIdOrZero).bind(userIdOrZero);
  std::vector<Loan> out;
  while (s.step()) {
    Loan l;
    l.id = s.i(0); l.userId = s.i(1); l.bookId = s.i(2); l.title = s.t(3); l.userName = s.t(4);
    l.borrowedAt = s.i(5); l.dueAt = s.i(6); l.overdue = l.dueAt < now;
    out.push_back(l);
  }
  return out;
}
std::vector<Loan> Library::activeLoans(int64_t userId) { return queryLoans(userId); }
std::vector<Loan> Library::allActiveLoans() { return queryLoans(0); }

void Library::addToCart(int64_t userId, int64_t bookId, int qty) {
  if (qty <= 0) throw LibraryError("INVALID_QUANTITY", "Số lượng phải lớn hơn 0", 400);
  const int64_t stock = db_.scalar("SELECT stock_for_sale FROM books WHERE id=?", -1, bookId);
  if (stock < 0) throw LibraryError("BOOK_NOT_FOUND", "Không tìm thấy sách", 404);
  const int64_t inCart = db_.scalar("SELECT qty FROM cart_items WHERE user_id=? AND book_id=?", 0, userId, bookId);
  if (inCart + qty > stock) throw LibraryError("OUT_OF_STOCK", "Không đủ hàng để bán", 409);
  Stmt s(db_,
         "INSERT INTO cart_items(user_id,book_id,qty) VALUES(?,?,?) "
         "ON CONFLICT(user_id,book_id) DO UPDATE SET qty=qty+excluded.qty");
  s.bind(userId).bind(bookId).bind(qty);
  s.run();
}

Cart Library::cart(int64_t userId) {
  Stmt s(db_,
         "SELECT b.id,b.title,b.price,c.qty FROM cart_items c JOIN books b ON b.id=c.book_id "
         "WHERE c.user_id=? ORDER BY b.title");
  s.bind(userId);
  Cart c;
  while (s.step()) {
    c.items.push_back({s.i(0), s.t(1), s.i(2), static_cast<int>(s.i(3))});
    c.total += s.i(2) * s.i(3);
  }
  return c;
}

Order Library::checkout(int64_t userId, const std::string& payment) {
  Tx tx(db_);
  const Cart c = cart(userId);
  if (c.items.empty()) throw LibraryError("EMPTY_CART", "Giỏ hàng trống", 400);
  for (const auto& it : c.items)
    if (db_.scalar("SELECT stock_for_sale FROM books WHERE id=?", 0, it.bookId) < it.qty)
      throw LibraryError("OUT_OF_STOCK", "Không đủ hàng: " + it.title, 409);
  if (payment == "declined") throw LibraryError("PAYMENT_DECLINED", "Thanh toán bị từ chối", 400);

  const int64_t now = clock_();
  {
    Stmt s(db_, "INSERT INTO orders(user_id,total,created_at) VALUES(?,?,?)");
    s.bind(userId).bind(c.total).bind(now);
    s.run();
  }
  const int64_t orderId = db_.lastInsertId();
  {
    Stmt s(db_,
           "INSERT INTO order_items(order_id,book_id,qty,price) SELECT ?,c.book_id,c.qty,b.price "
           "FROM cart_items c JOIN books b ON b.id=c.book_id WHERE c.user_id=?");
    s.bind(orderId).bind(userId);
    s.run();
  }
  for (const auto& it : c.items) {
    Stmt s(db_, "UPDATE books SET stock_for_sale=stock_for_sale-? WHERE id=?");
    s.bind(it.qty).bind(it.bookId);
    s.run();
  }
  {
    Stmt s(db_, "DELETE FROM cart_items WHERE user_id=?");
    s.bind(userId);
    s.run();
  }
  tx.commit();
  return {orderId, c.total, now};
}

void Library::clearCart(int64_t userId) {
  Stmt s(db_, "DELETE FROM cart_items WHERE user_id=?");
  s.bind(userId);
  s.run();
}
