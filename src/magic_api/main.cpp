#include <cstdlib>
#include <iostream>
#include <memory>

#include "magic_api/config.hpp"
#include "magic_api/routes.hpp"
#include "magic_api/server.hpp"
#include "magic_core/content_extractor.hpp"
#include "magic_core/metadata_store.hpp"
#include "magic_core/ollama_client.hpp"

int main() {
  try {
    Config config = Config::from_environment();

    std::string server_url = config.api_base_url;
    std::string metadata_path = config.metadata_db_path;
    std::string ollama_server_url = config.ollama_url;
    std::string model = config.embedding_model;

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