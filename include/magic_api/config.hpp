#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

class Config {
 public:
  std::string api_base_url;
  std::string metadata_db_path;
  std::string ollama_url;
  std::string embedding_model;

  static Config from_environment() {
    Config config;

    config.api_base_url = get_env_or_default("API_BASE_URL", "127.0.0.1:3030");
    config.metadata_db_path = get_env_or_default("METADATA_DB_PATH", "./data/metadata.db");
    config.ollama_url = get_env_or_default("OLLAMA_URL", "http://localhost:11434");
    config.embedding_model = get_env_or_default("EMBEDDING_MODEL", "nomic-embed-text");

    config.validate();
    return config;
  }

 private:
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