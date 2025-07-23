#pragma once

#include <filesystem>
#include <magic_core/db/metadata_store.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace magic_core {

class FileInfoService {
 public:
  explicit FileInfoService(std::shared_ptr<magic_core::MetadataStore> metadata_store);

  // Convenience wrappers over MetadataStore.
  std::vector<magic_core::FileMetadata> list_files();
  std::optional<magic_core::FileMetadata> get_file_info(const std::filesystem::path &file_path);

 private:
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
};

}