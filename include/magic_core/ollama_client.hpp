#pragma once

#include <curl/curl.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace magic_core {

class OllamaError : public std::exception {
 public:
  explicit OllamaError(const std::string &message) : message_(message) {}

  const char *what() const noexcept override {
    return message_.c_str();
  }

 private:
  std::string message_;
};

class OllamaClient {
 public:
  OllamaClient(const std::string &ollama_url, const std::string &embedding_model);
  ~OllamaClient();

  // Disable copy constructor and assignment
  OllamaClient(const OllamaClient &) = delete;
  OllamaClient &operator=(const OllamaClient &) = delete;

  // Allow move constructor and assignment
  OllamaClient(OllamaClient &&) noexcept;
  OllamaClient &operator=(OllamaClient &&) noexcept;

  // Get embedding for text
  std::vector<float> get_embedding(const std::string &text);

 private:
  std::string ollama_url_;
  std::string embedding_model_;
  CURL *curl_handle_;

  // Helper methods
  static size_t write_callback(void *contents, size_t size, size_t nmemb, std::string *userp);
  void setup_curl_handle();
};

}  // namespace magic_core