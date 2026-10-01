#include "recommender.hpp"

#include <algorithm>

std::vector<Scored> rankBooks(const std::vector<Candidate>& candidates, const History& h,
                              const std::string& topic, size_t limit, Weights w) {
  std::vector<const Candidate*> pool;
  int64_t maxLoans = 0;
  for (const auto& c : candidates) {
    if (h.borrowedBookIds.count(c.bookId)) continue;
    pool.push_back(&c);
    maxLoans = std::max(maxLoans, c.loanCount);
  }

  std::vector<Scored> out;
  for (const Candidate* c : pool) {
    Scored s;
    s.book = *c;
    s.topicScore = (!topic.empty() && c->topic == topic) ? 1.0 : 0.0;
    const double authorSeen = h.authors.count(c->author) ? 1.0 : 0.0;
    double topicShare = 0.0;
    if (h.total > 0) {
      auto it = h.topicCounts.find(c->topic);
      if (it != h.topicCounts.end()) topicShare = static_cast<double>(it->second) / h.total;
    }
    s.historyScore = 0.6 * authorSeen + 0.4 * topicShare;
    s.popularityScore = maxLoans > 0 ? static_cast<double>(c->loanCount) / maxLoans : 0.0;
    s.score = w.topic * s.topicScore + w.history * s.historyScore + w.popularity * s.popularityScore;
    out.push_back(s);
  }

  std::sort(out.begin(), out.end(), [](const Scored& a, const Scored& b) {
    if (a.score != b.score) return a.score > b.score;
    return a.book.title < b.book.title;
  });
  if (out.size() > limit) out.erase(out.begin() + static_cast<std::ptrdiff_t>(limit), out.end());
  return out;
}

std::vector<Scored> recommend(Database& db, int64_t userId, const std::string& topic, size_t limit) {
  std::vector<Candidate> cands;
  {
    Stmt s(db,
           "SELECT b.id,b.title,b.author,b.topic,(SELECT COUNT(*) FROM loans l WHERE l.book_id=b.id), b.image_url "
           "FROM books b WHERE b.copies_available>0");
    while (s.step()) cands.push_back({s.i(0), s.t(1), s.t(2), s.t(3), s.i(4), s.t(5)});
  }
  History h;
  {
    Stmt s(db, "SELECT b.id,b.author,b.topic FROM loans l JOIN books b ON b.id=l.book_id WHERE l.user_id=?");
    s.bind(userId);
    while (s.step()) {
      h.borrowedBookIds.insert(s.i(0));
      h.authors.insert(s.t(1));
      h.topicCounts[s.t(2)]++;
      h.total++;
    }
  }
  return rankBooks(cands, h, topic, limit);
}
