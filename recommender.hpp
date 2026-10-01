#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "database.hpp"

struct Candidate {  // a book that could be recommended
  int64_t bookId = 0;
  std::string title, author, topic;
  int64_t loanCount = 0;  // how often it has been borrowed (popularity)
  std::string imageUrl;
};

struct History {  // what this user has borrowed before
  std::set<int64_t> borrowedBookIds;
  std::set<std::string> authors;
  std::map<std::string, int> topicCounts;
  int total = 0;
};

struct Weights { double topic = 0.5, history = 0.3, popularity = 0.2; };

struct Scored {
  Candidate book;
  double score = 0, topicScore = 0, historyScore = 0, popularityScore = 0;
};

// Pure function (no database): easy to unit-test.
//   topicScore      1 if the book is in the topic the customer picked, else 0
//   historyScore    0.6 * (author already read) + 0.4 * (share of past loans in this book's topic)
//   popularityScore loans of this book / loans of the most-borrowed candidate
// Already-borrowed books are excluded. Ties are broken by title for stable output.
std::vector<Scored> rankBooks(const std::vector<Candidate>& candidates, const History& history,
                              const std::string& topic, size_t limit, Weights w = Weights{});

// Loads candidates (only books with a free copy) and the user's history, then ranks.
std::vector<Scored> recommend(Database& db, int64_t userId, const std::string& topic, size_t limit = 5);
