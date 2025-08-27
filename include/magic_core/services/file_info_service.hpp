#pragma once

#include <filesystem>
#include <magic_core/db/metadata_store.hpp>
#include <memory>
#include <optional>
#include <vector>
#include <functional>

namespace magic_core {

struct ChunkInfo {
  int id;
  int chunk_index;
  std::string content;
};

class FileInfoService {
 public:
  explicit FileInfoService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                          std::function<std::string(const std::vector<char>&)> decompress_fn = {});

  // Convenience wrappers over MetadataStore.
  std::vector<magic_core::FileMetadata> list_files();
  std::optional<magic_core::FileMetadata> get_file_info(const std::filesystem::path &file_path);
  
  // Get all chunks for a specific file by ID
  std::vector<ChunkInfo> get_file_chunks(int file_id);

 private:
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::function<std::string(const std::vector<char>&)> decompress_fn_;
};

}