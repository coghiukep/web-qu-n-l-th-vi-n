#pragma once
#include <stdexcept>
#include <string>
#include <utility>

// Every business-rule failure is a LibraryError with a stable machine-readable
// `code` (great for asserting in tests) and an HTTP status for the web layer.
class LibraryError : public std::runtime_error {
 public:
  LibraryError(std::string code, const std::string& message, int http = 400)
      : std::runtime_error(message), code(std::move(code)), http(http) {}
  std::string code;
  int http;
};
