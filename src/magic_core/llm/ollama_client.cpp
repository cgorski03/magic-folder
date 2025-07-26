#include "magic_core/llm/ollama_client.hpp"
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

// The Ollama api supports batch requests for the embeddings, this will have to be a separate endpoint
std::vector<float> OllamaClient::get_embedding(const std::string &text) {
  try {
    ollama::response response = ollama::generate_embeddings(embedding_model_, text);

    // Get the JSON structure
    auto json_response = response.as_json();

    // Check if embedding field exists
    if (!json_response.contains("embeddings")) {
      // Log the response itself
      throw OllamaError("Response does not contain embedding field");
    }

    // Handle different embedding response formats
    auto embeddings = json_response["embeddings"];
    if (embeddings.is_array()) {
      if (embeddings.size() > 0 && embeddings[0].is_array()) {
        // Array of arrays - take the first embedding vector
        return embeddings[0].get<std::vector<float>>();
      } else {
        // Single array of floats
        return embeddings.get<std::vector<float>>();
      }
    } else {
      throw OllamaError("Embeddings field is not an array");
    }

  } catch (const ollama::exception &e) {
    // Wrap ollama-hpp exceptions in your custom exception
    throw OllamaError("Embedding generation failed: " + std::string(e.what()));
  }
}

std::string OllamaClient::summarize_text(const std::string &text) {
  // TODO: Implement text summarization using ollama
  // For now, return a placeholder
  return "Summary of: " + text.substr(0, 100) + "...";
}

bool OllamaClient::is_server_available() {
  return ollama::is_running();
}

}  // namespace magic_core