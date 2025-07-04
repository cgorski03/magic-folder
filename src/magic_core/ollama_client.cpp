#include "magic_core/ollama_client.hpp"

#include <iostream>
#include <sstream>

namespace magic_core {

OllamaClient::OllamaClient(const std::string &ollama_url, const std::string &embedding_model)
    : ollama_url_(ollama_url), embedding_model_(embedding_model), curl_handle_(nullptr) {
  setup_curl_handle();
}

OllamaClient::~OllamaClient() {
  if (curl_handle_) {
    curl_easy_cleanup(curl_handle_);
  }
}

OllamaClient::OllamaClient(OllamaClient &&other) noexcept
    : ollama_url_(std::move(other.ollama_url_)),
      embedding_model_(std::move(other.embedding_model_)),
      curl_handle_(other.curl_handle_) {
  other.curl_handle_ = nullptr;
}

OllamaClient &OllamaClient::operator=(OllamaClient &&other) noexcept {
  if (this != &other) {
    if (curl_handle_) {
      curl_easy_cleanup(curl_handle_);
    }
    ollama_url_ = std::move(other.ollama_url_);
    embedding_model_ = std::move(other.embedding_model_);
    curl_handle_ = other.curl_handle_;
    other.curl_handle_ = nullptr;
  }
  return *this;
}

void OllamaClient::setup_curl_handle() {
  curl_handle_ = curl_easy_init();
  if (!curl_handle_) {
    throw OllamaError("Failed to initialize CURL");
  }
}

size_t OllamaClient::write_callback(void *contents, size_t size, size_t nmemb, std::string *userp) {
  userp->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::vector<float> OllamaClient::get_embedding(const std::string &text) {
  if (!curl_handle_) {
    throw OllamaError("CURL handle not initialized");
  }

  // Prepare JSON request
  nlohmann::json request_data = {{"model", embedding_model_}, {"prompt", text}};
  std::string request_json = request_data.dump();

  // Prepare response buffer
  std::string response_buffer;

  // Set up CURL options
  curl_easy_reset(curl_handle_);
  curl_easy_setopt(curl_handle_, CURLOPT_URL, (ollama_url_).c_str());
  curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, request_json.c_str());
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_buffer);
  curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER,
                   curl_slist_append(nullptr, "Content-Type: application/json"));

  // Perform request
  CURLcode res = curl_easy_perform(curl_handle_);
  if (res != CURLE_OK) {
    throw OllamaError("CURL request failed: " + std::string(curl_easy_strerror(res)));
  }

  // Check HTTP status code
  long http_code = 0;
  curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code != 200) {
    throw OllamaError("HTTP request failed with status code: " + std::to_string(http_code));
  }

  // Parse response
  try {
    nlohmann::json response = nlohmann::json::parse(response_buffer);
    if (!response.contains("embedding")) {
      throw OllamaError("Response does not contain embedding field");
    }

    std::vector<float> embedding = response["embedding"].get<std::vector<float>>();
    return embedding;
  } catch (const nlohmann::json::exception &e) {
    throw OllamaError("Failed to parse JSON response: " + std::string(e.what()));
  }
}

}