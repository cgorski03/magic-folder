#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

class Config {
 public:
  std::string api_base_url;
  std::string metadata_db_path;
  std::string ollama_url;
  std::string embedding_model;
  int num_workers;
  static Config from_environment() {
    Config config;

    // Load .env file first
    load_env_file(".env");

    config.api_base_url = get_env_or_default("API_BASE_URL", "127.0.0.1:3030");
    config.metadata_db_path = get_env_or_default("METADATA_DB_PATH", "./data/metadata.db");
    config.ollama_url = get_env_or_default("OLLAMA_URL", "http://localhost:11434");
    config.embedding_model = get_env_or_default("EMBEDDING_MODEL", "mxbai-embed-large");
    config.num_workers = std::stoi(get_env_or_default("NUM_WORKERS", "1"));
    config.validate();
    return config;
  }

 private:
  static void load_env_file(const std::string& filename) {
    // custom implementation to avoid a pointless dependency
    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cout << "Warning: .env file not found, using system environment variables" << std::endl;
      return;
    }

    std::string line;
    while (std::getline(file, line)) {
      // Skip empty lines and comments
      if (line.empty() || line[0] == '#') {
        continue;
      }

      // Find the equals sign
      size_t pos = line.find('=');
      if (pos == std::string::npos) {
        continue;  // Skip malformed lines
      }

      std::string key = line.substr(0, pos);
      std::string value = line.substr(pos + 1);

      // Remove quotes if present
      if (value.length() >= 2 && value[0] == '"' && value[value.length() - 1] == '"') {
        value = value.substr(1, value.length() - 2);
      }

      // Only set if not already in environment
      if (std::getenv(key.c_str()) == nullptr) {
        setenv(key.c_str(), value.c_str(), 1);
      }
    }
  }

  static std::string get_env_or_default(const char* name, const std::string& default_value) {
    const char* value = std::getenv(name);
    if (!value) {
      std::cout << "Environment variable " << name
                << " is not defined, using default value: " << default_value << std::endl;
    }
    return value ? std::string(value) : default_value;
  }

  void validate() const {
    if (api_base_url.empty())
      throw std::runtime_error("API_BASE_URL cannot be empty");
    if (metadata_db_path.empty())
      throw std::runtime_error("METADATA_DB_PATH cannot be empty");
    if (ollama_url.empty())
      throw std::runtime_error("OLLAMA_URL cannot be empty");
    if (embedding_model.empty())
      throw std::runtime_error("EMBEDDING_MODEL cannot be empty");
  }
};