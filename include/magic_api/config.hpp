#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

class Config {
 public:
  std::string api_base_url;
  std::string metadata_db_path;
  std::string ollama_url;
  std::string embedding_model;
  int num_workers;
  
  // File watcher configuration
  std::string watch_directory;
  bool file_watcher_enabled;
  int file_watcher_settle_ms;
  int file_watcher_modify_quiesce_minutes;

  // Load configuration from a JSON file at the given path
  static Config from_file(const std::string& filename) {
    std::ifstream file_stream(filename);
    if (!file_stream.is_open()) {
      throw std::runtime_error("Failed to open config file: " + filename);
    }

    nlohmann::json json_config;
    try {
      file_stream >> json_config;
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Failed to parse JSON in config file '") + filename + "': " + e.what());
    }

    return from_json(json_config);
  }

  // Construct configuration from a JSON object (useful for tests)
  static Config from_json(const nlohmann::json& json_config) {
    Config config;

    // Apply defaults when keys are missing
    config.api_base_url = json_config.value("api_base_url", std::string("127.0.0.1:3030"));
    config.metadata_db_path = json_config.value("metadata_db_path", std::string("./data/metadata.db"));
    config.ollama_url = json_config.value("ollama_url", std::string("http://localhost:11434"));
    config.embedding_model = json_config.value("embedding_model", std::string("mxbai-embed-large"));
    
    // File watcher defaults
    config.watch_directory = json_config.value("watch_directory", std::string("./data/watch"));
    config.file_watcher_enabled = json_config.value("file_watcher_enabled", true);
    config.file_watcher_settle_ms = json_config.value("file_watcher_settle_ms", 1500);
    config.file_watcher_modify_quiesce_minutes = json_config.value("file_watcher_modify_quiesce_minutes", 5);

    // Handle integer with default and basic type safety
    try {
      if (json_config.contains("num_workers")) {
        config.num_workers = json_config.at("num_workers").get<int>();
      } else {
        config.num_workers = 1;
      }
    } catch (const std::exception&) {
      // Fallback to default if wrong type provided
      config.num_workers = 1;
    }

    config.validate();
    return config;
  }

 private:
  void validate() const {
    if (api_base_url.empty()) {
      throw std::runtime_error("api_base_url cannot be empty");
    }
    if (metadata_db_path.empty()) {
      throw std::runtime_error("metadata_db_path cannot be empty");
    }
    if (ollama_url.empty()) {
      throw std::runtime_error("ollama_url cannot be empty");
    }
    if (embedding_model.empty()) {
      throw std::runtime_error("embedding_model cannot be empty");
    }
    if (num_workers <= 0) {
      throw std::runtime_error("num_workers must be greater than 0");
    }
    if (file_watcher_enabled && watch_directory.empty()) {
      throw std::runtime_error("watch_directory cannot be empty when file_watcher_enabled is true");
    }
    if (file_watcher_settle_ms < 100) {
      throw std::runtime_error("file_watcher_settle_ms must be at least 100ms");
    }
    if (file_watcher_modify_quiesce_minutes < 1) {
      throw std::runtime_error("file_watcher_modify_quiesce_minutes must be at least 1 minute");
    }
  }
};