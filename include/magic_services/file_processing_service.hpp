#pragma once

#include <filesystem>
#include <magic_core/content_extractor.hpp>
#include <magic_core/metadata_store.hpp>
#include <magic_core/ollama_client.hpp>
#include <memory>

namespace magic_services {

class FileProcessingService {
 public:
  FileProcessingService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                        std::shared_ptr<magic_core::ContentExtractor> content_extractor,
                        std::shared_ptr<magic_core::OllamaClient> ollama_client);

  // Ingest or update a single file, extracting content & embeddings.
  void process_file(const std::filesystem::path &file_path);

 private:
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::shared_ptr<magic_core::ContentExtractor> content_extractor_;
  std::shared_ptr<magic_core::OllamaClient> ollama_client_;
};

}