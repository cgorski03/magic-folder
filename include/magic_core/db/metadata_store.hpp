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
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "magic_core/types/chunk.hpp"
#include "magic_core/types/file.hpp"

namespace magic_core {

struct BasicFileMetadata {
  int id = 0;
  std::string path;
  std::string original_path;
  std::string file_hash;
  std::chrono::system_clock::time_point last_modified;
  std::chrono::system_clock::time_point created_at;
  FileType file_type;
  size_t file_size = 0;
  std::string processing_status = "IDLE";
  std::string tags;
};

struct FileMetadata : public BasicFileMetadata {
  std::vector<float> summary_vector_embedding;
  std::string suggested_category;
  std::string suggested_filename;
};

struct ChunkMetadata {
  int id;
  std::vector<float> vector_embedding;
  int file_id;
  int chunk_index;
  std::string content;
};
struct SearchResult {
  int id;
  float distance;
};

struct FileSearchResult : public SearchResult {
  FileMetadata file;
};

struct ChunkSearchResult : public SearchResult {
  int file_id;
  int chunk_index;
  std::string content;
};

struct ChunkWithEmbedding {
  Chunk chunk;
  std::vector<float> embedding;
};

enum class ProcessingStatus { IDLE, PROCESSING, FAILED };
inline std::string to_string(ProcessingStatus status) {
  switch (status) {
    case ProcessingStatus::IDLE:
      return "IDLE";
    case ProcessingStatus::PROCESSING:
      return "PROCESSING";
    case ProcessingStatus::FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

inline ProcessingStatus processing_status_from_string(const std::string &str) {
  if (str == "IDLE")
    return ProcessingStatus::IDLE;
  if (str == "PROCESSING")
    return ProcessingStatus::PROCESSING;
  if (str == "FAILED")
    return ProcessingStatus::FAILED;
  throw std::invalid_argument("Unknown ProcessingStatus: " + str);
}

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
  static constexpr int VECTOR_DIMENSION = 1024;

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

  int upsert_file_stub(const BasicFileMetadata &basic_metadata);

  void update_file_ai_analysis(int file_id,
                               const std::vector<float> &summary_vector,
                               const std::string &suggested_category = "",
                               const std::string &suggested_filename = "",
                               ProcessingStatus processing_status = ProcessingStatus::IDLE);

  void upsert_chunk_metadata(int file_id, const std::vector<ChunkWithEmbedding> &chunks);

  std::vector<ChunkMetadata> get_chunk_metadata(std::vector<int> file_ids);

  void fill_chunk_metadata(std::vector<ChunkSearchResult>& chunks);

  std::optional<FileMetadata> get_file_metadata(const std::string &path);

  std::optional<FileMetadata> get_file_metadata(int id);

  // Delete file metadata
  void delete_file_metadata(const std::string &path);

  // List all files
  std::vector<FileMetadata> list_all_files();

  // Check if file exists
  bool file_exists(const std::string &path);

  std::vector<FileSearchResult> search_similar_files(const std::vector<float> &query_vector, int k);
  std::vector<ChunkSearchResult> search_similar_chunks(const std::vector<int> &file_ids,
                                                       const std::vector<float> &query_vector,
                                                       int k);

  void rebuild_faiss_index();

 private:
  std::filesystem::path db_path_;
  sqlite3 *db_;

  // In-memory Faiss index
  faiss::IndexIDMap *faiss_index_;

  // Faiss Index Parameters - since we will support multiple embedding models, these will have to be
  // able to change
  const int HNSW_M_PARAM = 32;
  const int HNSW_EF_CONSTRUCTION_PARAM = 100;
  // Helper methods
  
  faiss::IndexIDMap *create_base_index();
  void search_faiss_index(faiss::IndexIDMap *index, const std::vector<float> &query_vector, int k, std::vector<float> &results, std::vector<faiss::idx_t> &labels);

  void create_tables();
  void execute_sql(const std::string &sql);
  std::chrono::system_clock::time_point get_file_last_modified(
      const std::filesystem::path &file_path);
  // Time point conversions
  std::string time_point_to_string(const std::chrono::system_clock::time_point &tp);
  std::chrono::system_clock::time_point string_to_time_point(const std::string &time_str);
  std::string int_vector_to_comma_string(const std::vector<int> &vector);
};
}  // namespace magic_core