#include <crow.h>

#include <iostream>

#include "auth.hpp"
#include "library.hpp"
#include "recommender.hpp"

namespace {

using crow::json::wvalue;

crow::response errorResp(int http, const std::string& code, const std::string& msg) {
  wvalue j;
  j["error"] = code;
  j["message"] = msg;
  return crow::response(http, std::move(j));
}

// Runs a handler; converts LibraryError into a JSON error with the right HTTP status.
template <class F>
crow::response guard(F&& f) {
  try {
    return f();
  } catch (const LibraryError& e) {
    return errorResp(e.http, e.code, e.what());
  } catch (const std::exception& e) {
    std::cerr << "internal error: " << e.what() << "\n";
    return errorResp(500, "INTERNAL", "Lỗi máy chủ");
  }
}

User currentUser(Auth& auth, const crow::request& req, bool librarianOnly = false) {
  const std::string h = req.get_header_value("Authorization"), prefix = "Bearer ";
  if (h.rfind(prefix, 0) != 0) throw LibraryError("UNAUTHORIZED", "Chưa đăng nhập", 401);
  auto u = auth.userFromToken(h.substr(prefix.size()));
  if (!u) throw LibraryError("UNAUTHORIZED", "Phiên đăng nhập không hợp lệ", 401);
  if (librarianOnly && u->role != "librarian") throw LibraryError("FORBIDDEN", "Chỉ thủ thư mới được phép", 403);
  return *u;
}

crow::json::rvalue parseBody(const crow::request& req) {
  auto b = crow::json::load(req.body);
  if (!b || b.t() != crow::json::type::Object) throw LibraryError("BAD_JSON", "Dữ liệu gửi lên không phải JSON object", 400);
  return b;
}
std::string getStr(const crow::json::rvalue& b, const char* k, bool required = true) {
  if (b.has(k) && b[k].t() == crow::json::type::String) return std::string(b[k].s());
  if (required) throw LibraryError("BAD_REQUEST", std::string("Thiếu trường văn bản: ") + k, 400);
  return "";
}
int64_t getInt(const crow::json::rvalue& b, const char* k) {
  if (b.has(k) && b[k].t() == crow::json::type::Number) return b[k].i();
  throw LibraryError("BAD_REQUEST", std::string("Thiếu trường số: ") + k, 400);
}
std::string param(const crow::request& req, const char* k) {
  const char* v = req.url_params.get(k);
  return v ? v : "";
}

wvalue userJson(const User& u) {
  wvalue j;
  j["id"] = u.id; j["email"] = u.email; j["name"] = u.name; j["role"] = u.role;
  return j;
}
wvalue bookJson(const Book& b) {
  wvalue j;
  j["id"] = b.id; j["title"] = b.title; j["author"] = b.author; j["topic"] = b.topic;
  j["image_url"] = b.imageUrl; j["price"] = b.price; j["copies_available"] = b.copiesAvailable; j["stock_for_sale"] = b.stockForSale;
  return j;
}
wvalue loanJson(const Loan& l) {
  wvalue j;
  j["id"] = l.id; j["book_id"] = l.bookId; j["title"] = l.title; j["user"] = l.userName;
  j["borrowed_at"] = l.borrowedAt; j["due_at"] = l.dueAt; j["overdue"] = l.overdue;
  return j;
}
template <class T, class F>
wvalue listJson(const std::vector<T>& v, F&& f) {
  std::vector<wvalue> arr;
  for (const auto& x : v) arr.push_back(f(x));
  return wvalue(std::move(arr));
}

void seedDemoData(Database& db, Auth& auth, Library& lib) {
  if (db.scalar("SELECT COUNT(*) FROM users WHERE role='librarian'", 0) == 0)
    auth.createUser("admin@thuvien.local", "Thủ thư", "admin12345", "librarian");
  if (db.scalar("SELECT COUNT(*) FROM books", 0) > 0) return;
  const std::vector<Book> demo = {
      {0, "Dune", "Frank Herbert", "Khoa học viễn tưởng", 120000, 2, 5},
      {0, "Foundation", "Isaac Asimov", "Khoa học viễn tưởng", 95000, 3, 4},
      {0, "Sherlock Holmes toàn tập", "Arthur Conan Doyle", "Trinh thám", 150000, 2, 3},
      {0, "Án mạng trên chuyến tàu tốc hành Phương Đông", "Agatha Christie", "Trinh thám", 89000, 2, 5},
      {0, "Sapiens", "Yuval Noah Harari", "Lịch sử", 180000, 3, 6},
      {0, "Súng, vi trùng và thép", "Jared Diamond", "Lịch sử", 160000, 1, 2},
      {0, "The C Programming Language", "Kernighan & Ritchie", "Lập trình", 250000, 2, 3},
      {0, "Clean Code", "Robert C. Martin", "Lập trình", 220000, 3, 4},
      {0, "Effective Modern C++", "Scott Meyers", "Lập trình", 240000, 1, 2},
      {0, "Nghĩ nhanh và chậm", "Daniel Kahneman", "Tâm lý", 170000, 2, 4},
      {0, "Đắc nhân tâm", "Dale Carnegie", "Tâm lý", 80000, 4, 8},
      {0, "Tôi thấy hoa vàng trên cỏ xanh", "Nguyễn Nhật Ánh", "Văn học", 95000, 3, 5},
  };
  for (const auto& b : demo) lib.addBook(b);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dbPath = argc > 1 ? argv[1] : "library.db";
  // One connection, single-threaded server: no locking needed for a learning project.
  Database db(dbPath);
  db.initSchema();
  Auth auth(db);
  Library lib(db);
  seedDemoData(db, auth, lib);

  crow::SimpleApp app;

  CROW_ROUTE(app, "/")([] {
    crow::response r;
    r.set_static_file_info("static/index.html");
    return r;
  });

  CROW_ROUTE(app, "/api/register").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      auto b = parseBody(req);
      const std::string email = getStr(b, "email");
      std::string name = getStr(b, "name", false);  // optional: defaults to the part before '@'
      if (name.empty()) name = email.substr(0, email.find('@'));
      User u = auth.registerUser(email, name, getStr(b, "password"));
      return crow::response(201, userJson(u));
    });
  });

  CROW_ROUTE(app, "/api/login").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      auto b = parseBody(req);
      std::string token = auth.login(getStr(b, "email"), getStr(b, "password"));
      wvalue j;
      j["token"] = token;
      j["user"] = userJson(*auth.userFromToken(token));
      return crow::response(200, std::move(j));
    });
  });

  CROW_ROUTE(app, "/api/logout").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      currentUser(auth, req);
      auth.logout(req.get_header_value("Authorization").substr(7));
      return crow::response(204);
    });
  });

  CROW_ROUTE(app, "/api/me")([&](const crow::request& req) {
    return guard([&] { return crow::response(200, userJson(currentUser(auth, req))); });
  });

  CROW_ROUTE(app, "/api/topics")([&] {
    return guard([&] {
      return crow::response(200, listJson(lib.topics(), [](const std::string& t) { return wvalue(t); }));
    });
  });

  CROW_ROUTE(app, "/api/books")([&](const crow::request& req) {
    return guard([&] {
      return crow::response(200, listJson(lib.listBooks(param(req, "topic"), param(req, "q")), bookJson));
    });
  });

  CROW_ROUTE(app, "/api/books").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      currentUser(auth, req);  // any signed-in user can add a book
      auto b = parseBody(req);
      Book book;
      book.title = getStr(b, "title"); book.author = getStr(b, "author"); book.topic = getStr(b, "topic");
      book.imageUrl = getStr(b, "image_url", false);
      book.price = getInt(b, "price");
      book.copiesAvailable = static_cast<int>(getInt(b, "copies_available"));
      book.stockForSale = static_cast<int>(getInt(b, "stock_for_sale"));
      book.id = lib.addBook(book);
      return crow::response(201, bookJson(book));
    });
  });

  CROW_ROUTE(app, "/api/recommendations")([&](const crow::request& req) {
    return guard([&] {
      User u = currentUser(auth, req);
      size_t limit = 5;
      try { limit = static_cast<size_t>(std::clamp(std::stoi(param(req, "limit")), 1, 20)); } catch (...) {}
      auto recs = recommend(db, u.id, param(req, "topic"), limit);
      return crow::response(200, listJson(recs, [](const Scored& s) {
        wvalue j;
        j["id"] = s.book.bookId; j["title"] = s.book.title; j["author"] = s.book.author; j["topic"] = s.book.topic;
        j["image_url"] = s.book.imageUrl; j["score"] = s.score; j["topic_score"] = s.topicScore;
        j["history_score"] = s.historyScore; j["popularity_score"] = s.popularityScore;
        return j;
      }));
    });
  });

  CROW_ROUTE(app, "/api/borrow").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      User u = currentUser(auth, req);
      Loan l = lib.borrow(u.id, getInt(parseBody(req), "book_id"));
      wvalue j;
      j["loan_id"] = l.id; j["due_at"] = l.dueAt;
      return crow::response(201, std::move(j));
    });
  });

  CROW_ROUTE(app, "/api/return").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      User u = currentUser(auth, req);
      wvalue j;
      j["late_fee"] = lib.returnBook(u.id, getInt(parseBody(req), "loan_id"));
      return crow::response(200, std::move(j));
    });
  });

  CROW_ROUTE(app, "/api/loans")([&](const crow::request& req) {
    return guard([&] {
      return crow::response(200, listJson(lib.activeLoans(currentUser(auth, req).id), loanJson));
    });
  });

  CROW_ROUTE(app, "/api/admin/loans")([&](const crow::request& req) {
    return guard([&] {
      currentUser(auth, req, true);
      return crow::response(200, listJson(lib.allActiveLoans(), loanJson));
    });
  });

  auto cartJson = [](const Cart& c) {
    wvalue j;
    j["items"] = listJson(c.items, [](const CartItem& it) {
      wvalue x;
      x["book_id"] = it.bookId; x["title"] = it.title; x["price"] = it.price; x["qty"] = it.qty;
      return x;
    });
    j["total"] = c.total;
    return j;
  };

  CROW_ROUTE(app, "/api/cart")([&](const crow::request& req) {
    return guard([&] { return crow::response(200, cartJson(lib.cart(currentUser(auth, req).id))); });
  });

  CROW_ROUTE(app, "/api/cart").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      User u = currentUser(auth, req);
      auto b = parseBody(req);
      lib.addToCart(u.id, getInt(b, "book_id"), static_cast<int>(getInt(b, "qty")));
      return crow::response(200, cartJson(lib.cart(u.id)));
    });
  });

  CROW_ROUTE(app, "/api/cart/clear").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      User u = currentUser(auth, req);
      lib.clearCart(u.id);
      return crow::response(200, cartJson(lib.cart(u.id)));
    });
  });

  CROW_ROUTE(app, "/api/checkout").methods("POST"_method)([&](const crow::request& req) {
    return guard([&] {
      User u = currentUser(auth, req);
      auto b = crow::json::load(req.body);
      std::string payment = (b && b.has("payment") && b["payment"].t() == crow::json::type::String)
                                ? std::string(b["payment"].s()) : "card";
      Order o = lib.checkout(u.id, payment);
      wvalue j;
      j["order_id"] = o.id; j["total"] = o.total;
      return crow::response(201, std::move(j));
    });
  });

  std::cout << "Thư viện chạy tại http://localhost:18080  (thủ thư: admin@thuvien.local / admin12345)\n";
  app.port(18080).run();
}
