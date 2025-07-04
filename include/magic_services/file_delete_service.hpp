#pragma once

#include <filesystem>
#include <magic_core/metadata_store.hpp>
#include <memory>

namespace magic_services {

class FileDeleteService {
 public:
  explicit FileDeleteService(std::shared_ptr<magic_core::MetadataStore> metadata_store);

  // Delete a file and its associated metadata/embeddings.
  void delete_file(const std::filesystem::path &file_path);

 private:
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
};

}  // namespace magic_services