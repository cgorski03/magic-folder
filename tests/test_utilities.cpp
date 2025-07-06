#include "test_utilities.hpp"

#include <chrono>
#include <filesystem>

namespace magic_tests {

std::filesystem::path TestUtilities::create_temp_test_db() {
  auto temp_dir = std::filesystem::temp_directory_path() / "magic_folder_tests";
  std::filesystem::create_directories(temp_dir);

  // Generate unique filename using timestamp
  auto now = std::chrono::system_clock::now();
  auto timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

  return temp_dir / ("test_" + std::to_string(timestamp) + ".db");
}

void TestUtilities::cleanup_temp_db(const std::filesystem::path& db_path) {
  if (std::filesystem::exists(db_path)) {
    std::filesystem::remove(db_path);
  }

  // Also cleanup the parent directory if it's empty
  auto parent_dir = db_path.parent_path();
  if (std::filesystem::exists(parent_dir) && std::filesystem::is_empty(parent_dir)) {
    std::filesystem::remove(parent_dir);
  }
}

magic_core::FileMetadata TestUtilities::create_test_file_metadata(const std::string& path,
                                                                  const std::string& content_hash,
                                                                  magic_core::FileType file_type,
                                                                  size_t file_size,
                                                                  bool include_vector) {
  magic_core::FileMetadata metadata;
  metadata.path = path;
  metadata.content_hash = content_hash;
  metadata.file_type = file_type;
  metadata.file_size = file_size;

  auto now = std::chrono::system_clock::now();
  metadata.last_modified = now;
  metadata.created_at = now - std::chrono::hours(1);  // Created 1 hour ago

  if (include_vector) {
    // Create a deterministic 1024-dimension vector based on the path hash
    metadata.vector_embedding.resize(1024);
    std::hash<std::string> hasher;
    size_t path_hash = hasher(path);

    for (int i = 0; i < 1024; ++i) {
      metadata.vector_embedding[i] = static_cast<float>((path_hash + i) % 1000) / 1000.0f;
    }
  }

  return metadata;
}

std::vector<magic_core::FileMetadata> TestUtilities::create_test_dataset(int count,
                                                                         const std::string& prefix,
                                                                         bool include_vectors) {
  std::vector<magic_core::FileMetadata> dataset;
  dataset.reserve(count);

  for (int i = 0; i < count; ++i) {
    std::string path = prefix + "/file" + std::to_string(i) + ".txt";
    std::string hash = "hash" + std::to_string(i);

    dataset.push_back(create_test_file_metadata(path, hash, magic_core::FileType::Text, 1024 + i,
                                                include_vectors));
  }

  return dataset;
}

void TestUtilities::populate_metadata_store(std::shared_ptr<magic_core::MetadataStore> store,
                                            const std::vector<magic_core::FileMetadata>& files) {
  for (const auto& file : files) {
    store->upsert_file_metadata(file);
  }
}

}  // namespace magic_tests