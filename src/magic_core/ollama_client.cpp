#include "magic_core/ollama_client.hpp"

#include <iostream>
#include <sstream>

#include "ollama.hpp"

namespace magic_core {

OllamaClient::OllamaClient(const std::string &ollama_url, const std::string &embedding_model)
    : ollama_url_(ollama_url), embedding_model_(embedding_model) {
  setup_server_connection();
}

void OllamaClient::setup_server_connection() {
  // Set the server URL for ollama-hpp
  ollama::setServerURL(ollama_url_);

  // Your custom initialization logic
  if (!ollama::is_running()) {
    throw OllamaError("Ollama server is not running at " + ollama_url_);
  }
}

std::vector<float> OllamaClient::get_embedding(const std::string &text) {
  try {
    // Use ollama-hpp for the actual work
    auto response = ollama::generate_embeddings(embedding_model_, text);

    // Extract embedding from response
    auto json_response = response.as_json();
    if (!json_response.contains("embedding")) {
      throw OllamaError("Response does not contain embedding field");
    }

    return json_response["embedding"].get<std::vector<float>>();

  } catch (const ollama::exception &e) {
    // Wrap ollama-hpp exceptions in your custom exception
    throw OllamaError("Embedding generation failed: " + std::string(e.what()));
  }
}

std::string OllamaClient::generate_text(const std::string &prompt) {
  try {
    auto response = ollama::generate(embedding_model_, prompt);
    return response.as_simple_string();
  } catch (const ollama::exception &e) {
    throw OllamaError("Text generation failed: " + std::string(e.what()));
  }
}

std::string OllamaClient::chat(const std::string &message) {
  // Implementation of chat function
  throw OllamaError("Chat function not implemented");
}

bool OllamaClient::is_server_available() {
  return ollama::is_running();
}

void OllamaClient::set_retry_attempts(int attempts) {
  retry_attempts_ = attempts;
}

void OllamaClient::enable_caching(bool enable) {
  caching_enabled_ = enable;
}

}  // namespace magic_core