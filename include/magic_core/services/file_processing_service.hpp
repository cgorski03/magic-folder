#pragma once

#include <filesystem>
#include <magic_core/extractors/content_extractor_factory.hpp>
#include <magic_core/db/metadata_store.hpp>
#include <magic_core/llm/ollama_client.hpp>
#include <memory>

namespace magic_core {

struct ProcessFileResult {
  bool success;
  std::string error_message;
  std::string file_path;
  size_t file_size;
  std::string content_hash;
  std::string file_type;

  // Constructor for success
  static ProcessFileResult success_response(const std::string& path,
                                            size_t size,
                                            const std::string& hash,
                                            const std::string& type) {
    return {true, "", path, size, hash, type};
  }

  // Constructor for failure
  static ProcessFileResult failure_response(const std::string& error,
                                            const std::string& path = "") {
    return {false, error, path, 0, "", ""};
  }
};

class FileProcessingService {
 public:
  FileProcessingService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                        std::shared_ptr<magic_core::ContentExtractorFactory> content_extractor_factory,
                        std::shared_ptr<magic_core::OllamaClient> ollama_client);

  // Ingest or update a single file, extracting content & embeddings.
  ProcessFileResult process_file(const std::filesystem::path& file_path);

 private:
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::shared_ptr<magic_core::ContentExtractorFactory> content_extractor_factory_;
  std::shared_ptr<magic_core::OllamaClient> ollama_client_;
};

}