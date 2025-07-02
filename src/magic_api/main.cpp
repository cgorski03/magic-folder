#include <cstdlib>
#include <iostream>
#include <memory>

#include "magic_api/routes.hpp"
#include "magic_api/server.hpp"
#include "magic_core/content_extractor.hpp"
#include "magic_core/metadata_store.hpp"
#include "magic_core/ollama_client.hpp"

int main() {
  try {
    // Get configuration from environment variables
    const char *api_base_url = std::getenv("API_BASE_URL");
    const char *metadata_db_path = std::getenv("METADATA_DB_PATH");
    const char *ollama_url = std::getenv("OLLAMA_URL");
    const char *embedding_model = std::getenv("EMBEDDING_MODEL");

    // Set defaults if not provided
    std::string server_url = api_base_url ? api_base_url : "127.0.0.1:3030";
    std::string metadata_path = metadata_db_path ? metadata_db_path : "./data/metadata.db";
    std::string ollama_server_url = ollama_url ? ollama_url : "http://localhost:11434";
    std::string model = embedding_model ? embedding_model : "mxbai-embed-large";

    std::cout << "Starting Magic Folder API Server..." << std::endl;
    std::cout << "Server URL: " << server_url << std::endl;
    std::cout << "Metadata DB Path: " << metadata_path << std::endl;
    std::cout << "Ollama URL: " << ollama_server_url << std::endl;
    std::cout << "Embedding Model: " << model << std::endl;

    // Initialize core components
    auto ollama_client = std::make_shared<magic_core::OllamaClient>(ollama_server_url, model);
    auto metadata_store = std::make_shared<magic_core::MetadataStore>(metadata_path);
    auto content_extractor = std::make_shared<magic_core::ContentExtractor>();

    // Parse server URL
    size_t colon_pos = server_url.find(':');
    if (colon_pos == std::string::npos) {
      std::cerr << "Invalid server URL format. Expected host:port" << std::endl;
      return 1;
    }

    std::string host = server_url.substr(0, colon_pos);
    int port = std::stoi(server_url.substr(colon_pos + 1));

    // Create and configure server
    magic_api::Server server(host, port);

    // Create routes and register them
    magic_api::Routes routes(ollama_client, metadata_store, content_extractor);
    routes.register_routes(server);

    std::cout << "Server configured successfully. Starting..." << std::endl;

    // Start the server
    server.start();
  } catch (const std::exception &e) {
    std::cerr << "Error starting server: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}