#include "magic_core/services/file_processing_service.hpp"

#include "magic_core/db/metadata_store.hpp"
// #include "magic_core/services/compression_service.hpp" // not used here

namespace magic_core {

auto to_sys_time = [](std::filesystem::file_time_type ftime) {
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
};

FileProcessingService::FileProcessingService(
    std::shared_ptr<magic_core::MetadataStore> metadata_store,
    std::shared_ptr<magic_core::TaskQueueRepo> task_queue_repo,
    std::shared_ptr<magic_core::ContentExtractorFactory> content_extractor_factory,
    std::shared_ptr<magic_core::OllamaClient> ollama_client)
    : metadata_store_(metadata_store),
      task_queue_repo_(task_queue_repo),
      content_extractor_factory_(content_extractor_factory),
      ollama_client_(ollama_client) {}

// some ugly boilerplate
BasicFileMetadata FileProcessingService::create_file_stub(const std::filesystem::path& file_path,
                                                          FileType file_type,
                                                          std::string content_hash) {
  BasicFileMetadata basic_file_metadata;
  basic_file_metadata.path = file_path;
  basic_file_metadata.file_size = std::filesystem::file_size(file_path);
  basic_file_metadata.last_modified = to_sys_time(std::filesystem::last_write_time(file_path));
  basic_file_metadata.created_at = to_sys_time(std::filesystem::last_write_time(file_path));
  basic_file_metadata.original_path = file_path;
  basic_file_metadata.processing_status = ProcessingStatus::QUEUED;
  basic_file_metadata.tags = "";
  basic_file_metadata.file_type = file_type;
  basic_file_metadata.content_hash = content_hash;
  return basic_file_metadata;
}

// Acts as a preflight check to make sure the request is valid and we are not just wasting time
std::optional<long long> FileProcessingService::request_processing(
    const std::filesystem::path& file_path) {
  // Quick preflight: path must exist
  if (!std::filesystem::exists(file_path)) {
    return std::nullopt;
  }
  // Perform a quick check to see if we evenash.
  const magic_core::ContentExtractor& extractor =
      content_extractor_factory_->get_extractor_for(file_path);
  std::string content_hash = extractor.get_content_hash(file_path);

  auto requested_file_processing_status = metadata_store_->file_processing_status(content_hash);
  if (requested_file_processing_status.has_value() &&
      *requested_file_processing_status != ProcessingStatus::FAILED) {
    return std::nullopt;
  }
  metadata_store_->upsert_file_stub(
      create_file_stub(file_path, extractor.get_file_type(), content_hash));
  long long task_id = task_queue_repo_->create_task("PROCESS_FILE", file_path.string());
  return task_id;
}

}  // namespace magic_core