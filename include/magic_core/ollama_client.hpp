#pragma once

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
  virtual std::vector<float> get_embedding(const std::string &text);
  virtual bool is_server_available();

 private:
  std::string ollama_url_;
  std::string embedding_model_;

  // Helper methods
  void setup_server_connection();
};

}  // namespace magic_core