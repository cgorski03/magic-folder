#include "magic_services/file_delete_service.hpp"

namespace magic_services {

FileDeleteService::FileDeleteService(std::shared_ptr<magic_core::MetadataStore> metadata_store)
    : metadata_store_(metadata_store) {}

void FileDeleteService::delete_file(const std::filesystem::path &file_path) {
  // First, we need to delete the file from the sqllite table
  metadata_store_->delete_file_metadata(file_path);
  // Now we need to make sure to delete the file from the in memory vector store
}
}  // namespace magic_services