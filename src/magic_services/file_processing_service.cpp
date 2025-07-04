#include "magic_services/file_processing_service.hpp"

namespace magic_services {

// Converts a std::filesystem::file_time_type to std::chrono::system_clock::time_point
auto to_sys_time = [](std::filesystem::file_time_type ftime) {
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
};

FileProcessingService::FileProcessingService(
    std::shared_ptr<magic_core::MetadataStore> metadata_store,
    std::shared_ptr<magic_core::ContentExtractor> content_extractor,
    std::shared_ptr<magic_core::OllamaClient> ollama_client)
    : metadata_store_(metadata_store),
      content_extractor_(content_extractor),
      ollama_client_(ollama_client) {}

void FileProcessingService::process_file(const std::filesystem::path &file_path) {
  // First, we need to extract the content of the file
  magic_core::ExtractedContent content = content_extractor_->extract_content(file_path);
  // Now we need to get the embedding of the content
  std::vector<float> embedding = ollama_client_->get_embedding(content.text_content);
  // Now we need to upsert the file metadata
  magic_core::FileMetadata file_metadata;
  file_metadata.path = file_path;
  file_metadata.content_hash = content.content_hash;
  file_metadata.file_type = content.file_type;
  file_metadata.file_size = std::filesystem::file_size(file_path);
  file_metadata.vector_embedding = embedding;
  file_metadata.last_modified = to_sys_time(std::filesystem::last_write_time(file_path));
  file_metadata.created_at = to_sys_time(std::filesystem::last_write_time(file_path));
  file_metadata.id = 0;

  metadata_store_->upsert_file_metadata(file_metadata);
}
}  // namespace magic_services