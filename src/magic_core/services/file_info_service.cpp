#include "magic_core/services/file_info_service.hpp"
#include "magic_core/services/compression_service.hpp"

namespace magic_core {
// This is a basic wrapper right now, but eventually would provide necessary statistics and other
// business logic
FileInfoService::FileInfoService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                                std::function<std::string(const std::vector<char>&)> decompress_fn)
    : metadata_store_(metadata_store) {
  if (decompress_fn) {
    decompress_fn_ = std::move(decompress_fn);
  } else {
    decompress_fn_ = [](const std::vector<char>& data) {
      return CompressionService::decompress(data);
    };
  }
}

std::vector<magic_core::FileMetadata> FileInfoService::list_files() {
  return metadata_store_->list_all_files();
}

std::optional<magic_core::FileMetadata> FileInfoService::get_file_info(
    const std::filesystem::path &file_path) {
  return metadata_store_->get_file_metadata(file_path);
}

std::vector<ChunkInfo> FileInfoService::get_file_chunks(int file_id) {
  std::vector<int> file_ids = {file_id};
  std::vector<ChunkMetadata> chunk_metadata = metadata_store_->get_chunk_metadata(file_ids);
  
  std::vector<ChunkInfo> chunks;
  chunks.reserve(chunk_metadata.size());
  
  for (const auto& metadata : chunk_metadata) {
    ChunkInfo chunk;
    chunk.id = metadata.id;
    chunk.chunk_index = metadata.chunk_index;
    chunk.content = decompress_fn_(metadata.content);
    chunks.push_back(std::move(chunk));
  }
  
  return chunks;
}
}  // namespace magic_core