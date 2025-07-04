#include "magic_services/file_info_service.hpp"

namespace magic_services {
// This is a basic wrapper right now, but eventually would provide necessary statistics and other
// business logic
FileInfoService::FileInfoService(std::shared_ptr<magic_core::MetadataStore> metadata_store)
    : metadata_store_(metadata_store) {}

std::vector<magic_core::FileMetadata> FileInfoService::list_files() {
  return metadata_store_->list_all_files();
}

std::optional<magic_core::FileMetadata> FileInfoService::get_file_info(
    const std::filesystem::path &file_path) {
  return metadata_store_->get_file_metadata(file_path);
}
}  // namespace magic_services