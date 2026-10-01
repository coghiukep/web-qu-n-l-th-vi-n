#include "test_util.hpp"

namespace {
Candidate cand(int64_t id, const std::string& title, const std::string& author, const std::string& topic,
               int64_t loans = 0) {
  return Candidate{id, title, author, topic, loans};
}
std::vector<std::string> titles(const std::vector<Scored>& v) {
  std::vector<std::string> t;
  for (auto& s : v) t.push_back(s.book.title);
  return t;
}
}  // namespace

TEST(Rank, ChosenTopicBeatsPopularityOutsideTopic) {
  // topic weight 0.5 > popularity weight 0.2
  auto r = rankBooks({cand(1, "Hot", "A", "Lịch sử", 100), cand(2, "Quiet", "B", "Trinh thám", 0)}, {},
                     "Trinh thám", 5);
  EXPECT_EQ(titles(r), (std::vector<std::string>{"Quiet", "Hot"}));
}

TEST(Rank, PopularityOrdersBooksWithinTopic) {
  auto r = rankBooks({cand(1, "Few", "A", "T", 1), cand(2, "Many", "B", "T", 9)}, {}, "T", 5);
  EXPECT_EQ(titles(r), (std::vector<std::string>{"Many", "Few"}));
}

TEST(Rank, KnownAuthorBoostsBook) {
  History h;
  h.authors.insert("Doyle");
  h.topicCounts["T"] = 1;
  h.total = 1;
  h.borrowedBookIds.insert(100);
  auto r = rankBooks({cand(1, "Other", "Christie", "T", 0), cand(2, "FromDoyle", "Doyle", "T", 0)}, h, "T", 5);
  EXPECT_EQ(r[0].book.title, "FromDoyle");
  EXPECT_GT(r[0].historyScore, r[1].historyScore);
}

TEST(Rank, HistoryShareCountsWhenNoTopicChosen) {
  History h;
  h.topicCounts = {{"Lịch sử", 3}, {"Trinh thám", 1}};
  h.total = 4;
  auto r = rankBooks({cand(1, "Detective", "A", "Trinh thám"), cand(2, "History", "B", "Lịch sử")}, h, "", 5);
  EXPECT_EQ(r[0].book.title, "History");
  EXPECT_DOUBLE_EQ(r[0].topicScore, 0.0);
}

TEST(Rank, ExcludesAlreadyBorrowedBooks) {
  History h;
  h.borrowedBookIds.insert(1);
  auto r = rankBooks({cand(1, "Read", "A", "T", 50), cand(2, "New", "B", "T", 0)}, h, "T", 5);
  EXPECT_EQ(titles(r), (std::vector<std::string>{"New"}));
}

TEST(Rank, TiesBrokenByTitleAndLimitApplied) {
  auto r = rankBooks({cand(1, "C", "x", "T"), cand(2, "A", "x", "T"), cand(3, "B", "x", "T")}, {}, "T", 2);
  EXPECT_EQ(titles(r), (std::vector<std::string>{"A", "B"}));
}

TEST(Rank, EdgeCases) {
  EXPECT_TRUE(rankBooks({}, {}, "T", 5).empty());
  EXPECT_TRUE(rankBooks({cand(1, "A", "x", "T")}, {}, "T", 0).empty());
  auto r = rankBooks({cand(1, "A", "x", "T", 0)}, {}, "Không có", 5);  // no loans anywhere: no divide-by-zero
  ASSERT_EQ(r.size(), 1u);
  EXPECT_DOUBLE_EQ(r[0].score, 0.0);
}

TEST(Rank, ScoresStayWithinZeroAndOne) {
  History h;
  h.authors.insert("x");
  h.topicCounts["T"] = 2;
  h.total = 2;
  for (auto& s : rankBooks({cand(1, "A", "x", "T", 7), cand(2, "B", "y", "U", 3)}, h, "T", 5)) {
    EXPECT_GE(s.score, 0.0);
    EXPECT_LE(s.score, 1.0 + 1e-9);
  }
}

using RecommendDbTest = LibraryFixture;

TEST_F(RecommendDbTest, SkipsUnavailableAndAlreadyBorrowedBooks) {
  User u = addUser("an"), other = addUser("binh");
  int64_t read = addBook("Đã đọc", "A", "T", 2);
  int64_t taken = addBook("Hết bản", "B", "T", 1);
  addBook("Còn", "C", "T", 1);
  lib.borrow(u.id, read);
  lib.borrow(other.id, taken);  // last copy gone
  EXPECT_EQ(titles(recommend(db, u.id, "T")), (std::vector<std::string>{"Còn"}));
}

TEST_F(RecommendDbTest, NewUserWithoutHistoryStillGetsResults) {
  User u = addUser("moi");
  addBook("A", "x", "T", 1);
  addBook("B", "y", "U", 1);
  auto r = recommend(db, u.id, "T");
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r[0].book.title, "A");  // topic match first
}

TEST_F(RecommendDbTest, PopularityComesFromRealLoanCounts) {
  User u = addUser("an"), v = addUser("binh");
  int64_t hot = addBook("Hot", "x", "T", 3), cold = addBook("Cold", "y", "T", 3);
  (void)cold;
  lib.borrow(v.id, hot);
  Loan l = lib.borrow(v.id, addBook("Phụ", "z", "U", 1));
  lib.returnBook(v.id, l.id);
  auto r = recommend(db, u.id, "T");
  EXPECT_EQ(r[0].book.title, "Hot");
}
