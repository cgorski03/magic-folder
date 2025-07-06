#include "magic_core/ollama_client.hpp"

#include <iostream>
#include <sstream>
#include <thread>

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
    ollama::response response = ollama::generate_embeddings(embedding_model_, text);

    // Get the JSON structure
    auto json_response = response.as_json();

    // Check if embedding field exists
    if (!json_response.contains("embedding")) {
      throw OllamaError("Response does not contain embedding field");
    }

    // Convert JSON array to vector<float>
    return json_response["embedding"].get<std::vector<float>>();

  } catch (const ollama::exception &e) {
    // Wrap ollama-hpp exceptions in your custom exception
    throw OllamaError("Embedding generation failed: " + std::string(e.what()));
  }
}

bool OllamaClient::is_server_available() {
  return ollama::is_running();
}

}  // namespace magic_core