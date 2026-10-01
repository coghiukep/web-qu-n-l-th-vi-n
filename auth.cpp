#include "auth.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <random>

namespace {

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::string randomHex(size_t bytes) {
  std::random_device rd;
  std::string out;
  char buf[3];
  for (size_t i = 0; i < bytes; ++i) {
    std::snprintf(buf, sizeof buf, "%02x", static_cast<unsigned>(rd() & 0xff));
    out += buf;
  }
  return out;
}

// Salted + stretched. Fine for a learning project; use Argon2/bcrypt in production.
std::string hashPassword(const std::string& password, const std::string& salt) {
  std::string h = sha256hex(salt + password);
  for (int i = 0; i < 1000; ++i) h = sha256hex(h + salt);
  return h;
}

std::string normalizeEmail(std::string e) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  e.erase(e.begin(), std::find_if(e.begin(), e.end(), notSpace));
  e.erase(std::find_if(e.rbegin(), e.rend(), notSpace).base(), e.end());
  std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return std::tolower(c); });
  return e;
}

bool validEmail(const std::string& e) {
  auto at = e.find('@');
  if (e.size() > 254 || at == std::string::npos || at == 0 || e.find('@', at + 1) != std::string::npos) return false;
  auto dot = e.find('.', at);
  return dot != std::string::npos && dot > at + 1 && dot + 1 < e.size() &&
         e.find(' ') == std::string::npos;
}

}  // namespace

std::string sha256hex(const std::string& msg) {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::string m = msg;
  const uint64_t bitLen = static_cast<uint64_t>(msg.size()) * 8;
  m.push_back(static_cast<char>(0x80));
  while (m.size() % 64 != 56) m.push_back('\0');
  for (int i = 7; i >= 0; --i) m.push_back(static_cast<char>((bitLen >> (i * 8)) & 0xff));

  auto byteAt = [&](size_t i) { return static_cast<uint32_t>(static_cast<uint8_t>(m[i])); };
  for (size_t off = 0; off < m.size(); off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = byteAt(off + 4 * i) << 24 | byteAt(off + 4 * i + 1) << 16 | byteAt(off + 4 * i + 2) << 8 |
             byteAt(off + 4 * i + 3);
    for (int i = 16; i < 64; ++i) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
      uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  std::string out;
  char buf[9];
  for (uint32_t v : h) {
    std::snprintf(buf, sizeof buf, "%08x", v);
    out += buf;
  }
  return out;
}

User Auth::registerUser(const std::string& email, const std::string& name, const std::string& password) {
  return createUser(email, name, password, "customer");
}

User Auth::createUser(const std::string& emailIn, const std::string& name, const std::string& password,
                      const std::string& role) {
  const std::string email = normalizeEmail(emailIn);
  if (!validEmail(email)) throw LibraryError("INVALID_EMAIL", "Email không hợp lệ", 400);
  if (name.empty() || name.size() > 100) throw LibraryError("INVALID_NAME", "Tên không hợp lệ", 400);
  if (password.size() < 8) throw LibraryError("WEAK_PASSWORD", "Mật khẩu tối thiểu 8 ký tự", 400);
  if (db_.scalar("SELECT COUNT(*) FROM users WHERE email=?", 0, email) > 0)
    throw LibraryError("EMAIL_TAKEN", "Email đã được đăng ký", 409);

  const std::string salt = randomHex(16);
  Stmt s(db_, "INSERT INTO users(email,name,salt,pass_hash,role) VALUES(?,?,?,?,?)");
  s.bind(email).bind(name).bind(salt).bind(hashPassword(password, salt)).bind(role);
  s.run();
  return User{db_.lastInsertId(), email, name, role};
}

std::string Auth::login(const std::string& emailIn, const std::string& password) {
  Stmt s(db_, "SELECT id, salt, pass_hash FROM users WHERE email=?");
  s.bind(normalizeEmail(emailIn));
  // Same error for unknown email and wrong password: don't reveal which emails exist.
  if (!s.step() || hashPassword(password, s.t(1)) != s.t(2))
    throw LibraryError("BAD_CREDENTIALS", "Sai email hoặc mật khẩu", 401);
  const int64_t uid = s.i(0);
  const std::string token = randomHex(24);
  Stmt ins(db_, "INSERT INTO sessions(token,user_id) VALUES(?,?)");
  ins.bind(token).bind(uid);
  ins.run();
  return token;
}

std::optional<User> Auth::userFromToken(const std::string& token) {
  Stmt s(db_,
         "SELECT u.id,u.email,u.name,u.role FROM sessions x JOIN users u ON u.id=x.user_id WHERE x.token=?");
  s.bind(token);
  if (!s.step()) return std::nullopt;
  return User{s.i(0), s.t(1), s.t(2), s.t(3)};
}

void Auth::logout(const std::string& token) {
  Stmt s(db_, "DELETE FROM sessions WHERE token=?");
  s.bind(token);
  s.run();
}
