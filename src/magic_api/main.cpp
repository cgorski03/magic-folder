#include <cstdlib>
#include <iostream>
#include <memory>

#include "magic_api/config.hpp"
#include "magic_api/routes.hpp"
#include "magic_api/server.hpp"
#include "magic_core/db/metadata_store.hpp"
#include "magic_core/llm/ollama_client.hpp"
#include "magic_core/services/file_delete_service.hpp"
#include "magic_core/services/file_info_service.hpp"
#include "magic_core/services/file_processing_service.hpp"
#include "magic_core/services/search_service.hpp"
#include "magic_core/services/encryption_key_service.hpp"
#include "magic_core/extractors/content_extractor_factory.hpp"
#include "magic_core/async/worker_pool.hpp"

int main() {
  try {
    Config config = Config::from_environment();

    std::string server_url = config.api_base_url;
    std::string metadata_path = config.metadata_db_path;
    std::string ollama_server_url = config.ollama_url;
    std::string model = config.embedding_model;
    std::string db_key = magic_core::EncryptionKeyService::get_database_key();
    std::cout << "Starting Magic Folder API Server..." << std::endl;
    std::cout << "Server URL: " << server_url << std::endl;
    std::cout << "Metadata DB Path: " << metadata_path << std::endl;
    std::cout << "Ollama URL: " << ollama_server_url << std::endl;
    std::cout << "Embedding Model: " << model << std::endl;

    // Initialize core components
    auto ollama_client = std::make_shared<magic_core::OllamaClient>(ollama_server_url, model);
    auto metadata_store = std::make_shared<magic_core::MetadataStore>(metadata_path, db_key);
    auto content_extractor_factory = std::make_shared<magic_core::ContentExtractorFactory>();

    auto file_processing_service = std::make_shared<magic_core::FileProcessingService>(
        metadata_store, 
        content_extractor_factory,
        ollama_client
    );
    auto file_delete_service = std::make_shared<magic_core::FileDeleteService>(metadata_store);
    auto file_info_service = std::make_shared<magic_core::FileInfoService>(metadata_store);
    auto search_service =
        std::make_shared<magic_core::SearchService>(metadata_store, ollama_client);

    auto worker_pool = std::make_shared<magic_core::async::WorkerPool>(
        config.num_workers,
        *metadata_store,
        *ollama_client,
        *content_extractor_factory);

    worker_pool->start();
    // Parse server URL
    size_t colon_pos = server_url.find(':');
    if (colon_pos == std::string::npos) {
      std::cerr << "Invalid server URL format. Expected host:port" << std::endl;
      return 1;
    }

    std::string host = server_url.substr(0, colon_pos);
    int port = std::stoi(server_url.substr(colon_pos + 1));

    magic_api::Server server(host, port);

    // Create routes and register them
    magic_api::Routes routes(file_processing_service, file_delete_service, file_info_service,
                             search_service);
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