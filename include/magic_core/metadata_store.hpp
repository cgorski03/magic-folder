#pragma once

#include <faiss/Index.h>
#include <faiss/IndexFlat.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>
// Why is this file named with a different convention
#include <faiss/index_io.h>
#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "magic_core/types.hpp"
namespace magic_core {

struct FileMetadata {
  int id = 0;
  std::string path;
  std::string content_hash;
  std::chrono::system_clock::time_point last_modified;
  std::chrono::system_clock::time_point created_at;
  FileType file_type;
  size_t file_size = 0;
  std::vector<float> vector_embedding;
};

struct SearchResult {
  int id;
  float distance;
  std::string path;  // Include path directly for convenience in results
  // You could also add other relevant metadata fields here if desired,
  // or return the full FileMetadata object if that suits your needs better.
};

class MetadataStoreError : public std::exception {
 public:
  explicit MetadataStoreError(const std::string &message) : message_(message) {}

  const char *what() const noexcept override {
    return message_.c_str();
  }

 private:
  std::string message_;
};

class MetadataStore {
 public:
  explicit MetadataStore(const std::filesystem::path &db_path);
  ~MetadataStore();

  // Disable copy constructor and assignment
  MetadataStore(const MetadataStore &) = delete;
  MetadataStore &operator=(const MetadataStore &) = delete;

  // Allow move constructor and assignment
  MetadataStore(MetadataStore &&) noexcept;
  MetadataStore &operator=(MetadataStore &&) noexcept;

  // Initialize the database and build the Faiss index
  void initialize();

  // Add or update file metadata (now including vector embedding)
  void upsert_file_metadata(const FileMetadata &metadata);

  // Get file metadata by path
  std::optional<FileMetadata> get_file_metadata(const std::string &path);

  // Get file metadata by ID (Faiss returns IDs, so this is crucial)
  std::optional<FileMetadata> get_file_metadata(int id);

  // Delete file metadata
  void delete_file_metadata(const std::string &path);

  // List all files
  std::vector<FileMetadata> list_all_files();

  // Check if file exists
  bool file_exists(const std::string &path);

  std::vector<SearchResult> search_similar_files(const std::vector<float> &query_vector, int k);

  void rebuild_faiss_index();

  std::string compute_content_hash(const std::filesystem::path &file_path);

 private:
  std::filesystem::path db_path_;
  sqlite3 *db_;

  // In-memory Faiss index
  faiss::Index *faiss_index_;

  // Faiss Index Parameters - since we will support multiple embedding models, these will have to be
  // able to change
  const int VECTOR_DIMENSION = 1024;
  const int HNSW_M_PARAM = 32;
  const int HNSW_EF_CONSTRUCTION_PARAM = 100;

  // Helper methods
  void create_tables();
  void execute_sql(const std::string &sql);
  std::chrono::system_clock::time_point get_file_last_modified(
      const std::filesystem::path &file_path);
  // Time point conversions
  std::string time_point_to_string(const std::chrono::system_clock::time_point &tp);
  std::chrono::system_clock::time_point string_to_time_point(const std::string &time_str);
};
}  // namespace magic_core