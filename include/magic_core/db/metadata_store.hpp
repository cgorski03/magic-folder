#pragma once
#include <faiss/Index.h>
#include <faiss/IndexFlat.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>
// Why is this file named with a different convention
#include <faiss/index_io.h>
#include <sqlite_modern_cpp.h>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "magic_core/types/chunk.hpp"
#include "magic_core/types/file.hpp"
#include "magic_core/db/database_manager.hpp"

namespace magic_core {

// Task types/status and Task struct are declared in task_queue_repo.hpp

enum class ProcessingStatus { QUEUED, PROCESSED, PROCESSING, FAILED };
inline std::string to_string(ProcessingStatus status) {
  switch (status) {
    case ProcessingStatus::PROCESSED:
      return "PROCESSED";
    case ProcessingStatus::QUEUED:
      return "QUEUED";
    case ProcessingStatus::PROCESSING:
      return "PROCESSING";
    case ProcessingStatus::FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

inline ProcessingStatus processing_status_from_string(const std::string &str) {
  if (str == "PROCESSED")
    return ProcessingStatus::PROCESSED;
  if (str == "QUEUED")
    return ProcessingStatus::QUEUED;
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
struct BasicFileMetadata {
  int id = 0;
  std::string path;
  std::string original_path;
  std::string content_hash;
  std::chrono::system_clock::time_point last_modified;
  std::chrono::system_clock::time_point created_at;
  FileType file_type;
  size_t file_size = 0;
  ProcessingStatus processing_status = ProcessingStatus::PROCESSED;
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
  std::vector<char> content;
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
  std::vector<char> compressed_content;
};

struct ProcessedChunk {
  Chunk chunk;
  std::vector<char> compressed_content;
};

class MetadataStore {
 public:
  static constexpr int VECTOR_DIMENSION = 1024;
  explicit MetadataStore(DatabaseManager& db_manager);
  ~MetadataStore();

  // Disable copy constructor and assignment
  MetadataStore(const MetadataStore &) = delete;
  MetadataStore &operator=(const MetadataStore &) = delete;

  // Non-movable to keep DB references stable
  MetadataStore(MetadataStore &&) = delete;
  MetadataStore &operator=(MetadataStore &&) = delete;

  // Initialize the database and build the Faiss index
  void initialize();

  int upsert_file_stub(const BasicFileMetadata &basic_metadata);

  void update_file_ai_analysis(int file_id,
                               const std::vector<float> &summary_vector,
                               const std::string &suggested_category = "",
                               const std::string &suggested_filename = "",
                               ProcessingStatus processing_status = ProcessingStatus::PROCESSED);
  void update_file_processing_status(int file_id, ProcessingStatus processing_status);

  void upsert_chunk_metadata(int file_id, const std::vector<ProcessedChunk> &chunks);

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
  std::optional<ProcessingStatus> file_processing_status(std::string content_hash);

  std::vector<FileSearchResult> search_similar_files(const std::vector<float> &query_vector, int k);
  std::vector<ChunkSearchResult> search_similar_chunks(const std::vector<int> &file_ids,
                                                       const std::vector<float> &query_vector,
                                                       int k);

  void rebuild_faiss_index();

 private:
  sqlite::database& db_; // non-owning reference provided by DatabaseManager
    
  // In-memory Faiss index
  faiss::IndexIDMap *faiss_index_;

  // Faiss Index Parameters - since we will support multiple embedding models, these will have to be
  // able to change
  const int HNSW_M_PARAM = 32;
  const int HNSW_EF_CONSTRUCTION_PARAM = 100;
  // Helper methods
  
  faiss::IndexIDMap *create_base_index();
  void search_faiss_index(faiss::IndexIDMap *index, const std::vector<float> &query_vector, int k, std::vector<float> &results, std::vector<faiss::idx_t> &labels);

  std::chrono::system_clock::time_point get_file_last_modified(
      const std::filesystem::path &file_path);
  // Time point conversions
  std::string time_point_to_string(const std::chrono::system_clock::time_point &tp);
  std::chrono::system_clock::time_point string_to_time_point(const std::string &time_str);
  std::string int_vector_to_comma_string(const std::vector<int> &vector);
};
}  // namespace magic_core