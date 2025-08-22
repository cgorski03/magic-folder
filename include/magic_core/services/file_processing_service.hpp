#pragma once

#include <filesystem>
#include <magic_core/db/metadata_store.hpp>
#include <magic_core/db/task_queue_repo.hpp>
#include <magic_core/extractors/content_extractor_factory.hpp>
#include <magic_core/llm/ollama_client.hpp>
#include <memory>

namespace magic_core {

struct ProcessFileResult {
  bool success;
  std::string error_message;
  std::string file_path;
  size_t file_size;
  std::string content_hash;
  FileType file_type;

  // Constructor for success
  static ProcessFileResult success_response(const std::string& path,
                                            size_t size,
                                            const std::string& hash,
                                            FileType type) {
    return {true, "", path, size, hash, type};
  }

  // Constructor for failure
  static ProcessFileResult failure_response(const std::string& error,
                                            const std::string& path = "") {
    return {false, error, path, 0, "", FileType::Unknown};
  }
};

class FileProcessingService {
 public:
  FileProcessingService(
      std::shared_ptr<magic_core::MetadataStore> metadata_store,
      std::shared_ptr<magic_core::TaskQueueRepo> task_queue_repo,
      std::shared_ptr<magic_core::ContentExtractorFactory> content_extractor_factory,
      std::shared_ptr<magic_core::OllamaClient> ollama_client);
  
  virtual ~FileProcessingService() = default;
  
  // Request a file to be processed, if it's not already in the queue
  virtual std::optional<long long> request_processing(const std::filesystem::path& file_path);
  
 private:
  static BasicFileMetadata create_file_stub(const std::filesystem::path& file_path, FileType file_type, std::string content_hash);
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::shared_ptr<magic_core::TaskQueueRepo> task_queue_repo_;
  std::shared_ptr<magic_core::ContentExtractorFactory> content_extractor_factory_;
  std::shared_ptr<magic_core::OllamaClient> ollama_client_;
};
}  // namespace magic_core