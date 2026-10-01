#include "test_util.hpp"

TEST(Sha256, KnownVectors) {
  EXPECT_EQ(sha256hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(sha256hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  // 56 bytes: forces a second padding block
  EXPECT_EQ(sha256hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

using AuthTest = LibraryFixture;

TEST_F(AuthTest, RegisterCreatesCustomerWithNormalizedEmail) {
  User u = auth.registerUser("  Hieu@Example.COM ", "Hiếu", "password123");
  EXPECT_EQ(u.email, "hieu@example.com");
  EXPECT_EQ(u.role, "customer");
  EXPECT_GT(u.id, 0);
}

TEST_F(AuthTest, DuplicateEmailRejectedCaseInsensitive) {
  auth.registerUser("a@b.com", "A", "password123");
  EXPECT_ERR(auth.registerUser("A@B.com", "B", "password123"), "EMAIL_TAKEN");
}

TEST_F(AuthTest, RejectsBadInput) {
  EXPECT_ERR(auth.registerUser("not-an-email", "A", "password123"), "INVALID_EMAIL");
  EXPECT_ERR(auth.registerUser("a@b", "A", "password123"), "INVALID_EMAIL");
  EXPECT_ERR(auth.registerUser("a@@b.com", "A", "password123"), "INVALID_EMAIL");
  EXPECT_ERR(auth.registerUser("a@b.com", "", "password123"), "INVALID_NAME");
  EXPECT_ERR(auth.registerUser("a@b.com", "A", "short"), "WEAK_PASSWORD");
}

TEST_F(AuthTest, PasswordIsNotStoredInPlainText) {
  auth.registerUser("a@b.com", "A", "password123");
  Stmt s(db, "SELECT pass_hash FROM users WHERE email='a@b.com'");
  ASSERT_TRUE(s.step());
  EXPECT_NE(s.t(0), "password123");
  EXPECT_EQ(s.t(0).size(), 64u);
}

TEST_F(AuthTest, LoginReturnsWorkingTokenAndLogoutRevokesIt) {
  User u = auth.registerUser("a@b.com", "A", "password123");
  std::string token = auth.login("A@B.com", "password123");
  auto who = auth.userFromToken(token);
  ASSERT_TRUE(who.has_value());
  EXPECT_EQ(who->id, u.id);
  auth.logout(token);
  EXPECT_FALSE(auth.userFromToken(token).has_value());
}

TEST_F(AuthTest, WrongPasswordAndUnknownEmailLookIdentical) {
  auth.registerUser("a@b.com", "A", "password123");
  std::string m1, m2;
  try { auth.login("a@b.com", "wrongpass1"); } catch (const LibraryError& e) { m1 = e.code + e.what(); }
  try { auth.login("nobody@b.com", "password123"); } catch (const LibraryError& e) { m2 = e.code + e.what(); }
  EXPECT_FALSE(m1.empty());
  EXPECT_EQ(m1, m2);
}

TEST_F(AuthTest, EachLoginGetsDifferentToken) {
  auth.registerUser("a@b.com", "A", "password123");
  EXPECT_NE(auth.login("a@b.com", "password123"), auth.login("a@b.com", "password123"));
}

TEST_F(AuthTest, GarbageTokenIsRejected) {
  EXPECT_FALSE(auth.userFromToken("").has_value());
  EXPECT_FALSE(auth.userFromToken("' OR '1'='1").has_value());  // SQL injection attempt
}
