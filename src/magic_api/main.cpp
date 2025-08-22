#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#include "magic_api/config.hpp"
#include "magic_api/routes.hpp"
#include "magic_api/server.hpp"
#include "magic_core/async/file_watcher_service.hpp"
#include "magic_core/async/service_provider.hpp"
#include "magic_core/async/worker_pool.hpp"
#include "magic_core/db/database_manager.hpp"
#include "magic_core/db/metadata_store.hpp"
#include "magic_core/db/task_queue_repo.hpp"
#include "magic_core/extractors/content_extractor_factory.hpp"
#include "magic_core/llm/ollama_client.hpp"
#include "magic_core/services/encryption_key_service.hpp"
#include "magic_core/services/file_delete_service.hpp"
#include "magic_core/services/file_info_service.hpp"
#include "magic_core/services/file_processing_service.hpp"
#include "magic_core/services/search_service.hpp"

std::atomic<bool> shutdown_requested = false;
std::mutex shutdown_mutex;
std::condition_variable shutdown_cv;

// The signal handler function
void signal_handler(int signal) {
  std::cout << "\nShutdown signal (" << signal << ") received. Initiating graceful shutdown..."
            << std::endl;
  shutdown_requested = true;
  shutdown_cv.notify_one();  // Wake up the main thread
}
int main() {
  try {
    Config config = Config::from_file("magicrc.json");

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
    std::cout << "File Watcher Enabled: " << (config.file_watcher_enabled ? "Yes" : "No") << std::endl;
    if (config.file_watcher_enabled) {
      std::cout << "Watch Directory: " << config.watch_directory << std::endl;
    }

    // Initialize core components
    auto ollama_client = std::make_shared<magic_core::OllamaClient>(ollama_server_url, model);
    auto& db_manager = magic_core::DatabaseManager::get_instance();
    db_manager.initialize(metadata_path, db_key, /*pool_size*/ config.num_workers);
    auto metadata_store = std::make_shared<magic_core::MetadataStore>(db_manager);
    auto task_queue_repo = std::make_shared<magic_core::TaskQueueRepo>(db_manager);
    auto content_extractor_factory = std::make_shared<magic_core::ContentExtractorFactory>();

    auto file_processing_service = std::make_shared<magic_core::FileProcessingService>(
        metadata_store, task_queue_repo, content_extractor_factory, ollama_client);
    auto file_delete_service = std::make_shared<magic_core::FileDeleteService>(metadata_store);
    auto file_info_service = std::make_shared<magic_core::FileInfoService>(metadata_store);
    auto search_service =
        std::make_shared<magic_core::SearchService>(metadata_store, ollama_client);
    auto services = std::make_shared<magic_core::ServiceProvider>(
        metadata_store, task_queue_repo, ollama_client, content_extractor_factory);
    auto worker_pool =
        std::make_shared<magic_core::async::WorkerPool>(config.num_workers, services);

    // Initialize file watcher service if enabled
    std::unique_ptr<magic_core::async::FileWatcherService> file_watcher;
    if (config.file_watcher_enabled) {
      magic_core::async::WatchConfig watch_config;
      watch_config.drop_root = config.watch_directory;
      watch_config.recursive = true;
      watch_config.settle_ms = std::chrono::milliseconds(config.file_watcher_settle_ms);
      watch_config.modify_quiesce_ms = std::chrono::minutes(config.file_watcher_modify_quiesce_minutes);
      
      // Create watch directory if it doesn't exist
      std::error_code ec;
      std::filesystem::create_directories(watch_config.drop_root, ec);
      if (ec) {
        std::cerr << "Warning: Failed to create watch directory: " << ec.message() << std::endl;
      }
      
      file_watcher = std::make_unique<magic_core::async::FileWatcherService>(
          watch_config, *task_queue_repo, *metadata_store);
    }
    std::string host = server_url.substr(0, server_url.find(':'));
    int port = std::stoi(server_url.substr(server_url.find(':') + 1));
    magic_api::Server server(host, port);
    magic_api::Routes routes(file_processing_service, file_delete_service, file_info_service,
                             search_service, task_queue_repo);
    routes.register_routes(server);

    // --- 2. START BACKGROUND SERVICES ---
    std::cout << "Disabling Crow's internal signal handling..." << std::endl;
    server.get_app().signal_clear();

    worker_pool->start();
    
    // Start file watcher if enabled
    if (file_watcher) {
      std::cout << "Starting file watcher service..." << std::endl;
      file_watcher->start();
      // Perform initial scan to pick up existing files
      file_watcher->initial_scan();
      std::cout << "File watcher started and initial scan completed." << std::endl;
    }
    
    server.start();
    std::cout << "Server started successfully. Press Ctrl+C to exit." << std::endl;

    // --- 3. WAIT FOR SHUTDOWN SIGNAL ---
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    {
      std::unique_lock<std::mutex> lock(shutdown_mutex);
      shutdown_cv.wait(lock, [] { return shutdown_requested.load(); });
    }

    // --- 4. GRACEFUL SHUTDOWN SEQUENCE ---
    std::cout << "[1/4] Stopping API server to refuse new requests..." << std::endl;
    server.stop();

    std::cout << "[2/4] Stopping file watcher..." << std::endl;
    if (file_watcher) {
      file_watcher->stop();
    }

    std::cout << "[3/4] Stopping worker pool to finish processing..." << std::endl;
    worker_pool->stop();  // Blocks until all workers are done

    std::cout << "[4/4] Shutting down database connections..." << std::endl;
    db_manager.shutdown();

    std::cout << "Shutdown complete." << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Error starting server: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}