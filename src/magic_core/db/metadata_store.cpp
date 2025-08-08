#include "magic_core/db/metadata_store.hpp"

#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace magic_core {

std::string MetadataStore::time_point_to_string(const std::chrono::system_clock::time_point &tp) {
  auto time_t = std::chrono::system_clock::to_time_t(tp);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

std::chrono::system_clock::time_point MetadataStore::string_to_time_point(
    const std::string &time_str) {
  std::tm tm_struct = {};
  std::stringstream ss(time_str);
  // Parse time string in "YYYY-MM-DD HH:MM:SS" format
  ss >> std::get_time(&tm_struct, "%Y-%m-%d %H:%M:%S");
  if (ss.fail()) {
    throw std::runtime_error("Failed to parse time string: " + time_str +
                             ". Expected format YYYY-MM-DD HH:MM:SS.");
  }
  // We stored GMT time.
  return std::chrono::system_clock::from_time_t(timegm(&tm_struct));
}

std::chrono::system_clock::time_point MetadataStore::get_file_last_modified(
    const std::filesystem::path &file_path) {
  auto ftime = std::filesystem::last_write_time(file_path);
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
  return sctp;
}

MetadataStore::MetadataStore(const std::filesystem::path &db_path, const std::string &db_key)
    : db_path_(db_path), db_(nullptr), faiss_index_(nullptr) {
  try {
    std::filesystem::create_directories(db_path_.parent_path());

    db_ = std::make_unique<sqlite::database>(db_path_.string());

    sqlite3 *handle = db_->connection().get();
    if (!handle) {
      throw MetadataStoreError("Failed to get native database handle after opening.");
    }

    if (sqlite3_key(handle, db_key.c_str(), db_key.length()) != SQLITE_OK) {
      std::string error_msg = sqlite3_errmsg(handle);
      throw MetadataStoreError("Failed to key database: " + error_msg);
    }

    //    If the key is wrong, this will throw a sqlite::sqlite_exception.
    *db_ << "SELECT count(*) FROM sqlite_master;";

    // The rest of the initialization proceeds as normal
    *db_ << "PRAGMA foreign_keys = ON;";
    create_tables();
    rebuild_faiss_index();

  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to initialize database: " + std::string(e.what()));
  }
}
MetadataStore::~MetadataStore() {
  if (faiss_index_) {
    delete faiss_index_;
  }
}

MetadataStore::MetadataStore(MetadataStore &&other) noexcept
    : db_path_(std::move(other.db_path_)),
      db_(std::move(other.db_)),
      faiss_index_(other.faiss_index_) {
  other.faiss_index_ = nullptr;
}

MetadataStore &MetadataStore::operator=(MetadataStore &&other) noexcept {
  if (this != &other) {
    if (faiss_index_) {
      // Clean up current faiss index in memory
      delete faiss_index_;
    }
    db_path_ = std::move(other.db_path_);
    db_ = std::move(other.db_);
    faiss_index_ = other.faiss_index_;
    other.faiss_index_ = nullptr;
  }
  return *this;
}

void MetadataStore::initialize() {
  try {
    std::filesystem::create_directories(db_path_.parent_path());
    db_ = std::make_unique<sqlite::database>(db_path_.string());
    *db_ << "PRAGMA foreign_keys = ON;";

    create_tables();
    rebuild_faiss_index();
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to initialize database: " + std::string(e.what()));
  }
}

void MetadataStore::create_tables() {
  try {
    *db_ << R"(
      CREATE TABLE IF NOT EXISTS files (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          path TEXT UNIQUE NOT NULL,
          original_path TEXT,
          file_hash TEXT NOT NULL,
          processing_status TEXT DEFAULT 'IDLE',
          summary_vector_blob BLOB,
          suggested_category TEXT,
          suggested_filename TEXT,
          tags TEXT,
          last_modified TEXT NOT NULL,
          created_at TEXT NOT NULL,
          file_type TEXT NOT NULL,
          file_size INTEGER NOT NULL
      )
    )";

    *db_ << R"(
      CREATE TABLE IF NOT EXISTS chunks (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          file_id INTEGER NOT NULL,
          chunk_index INTEGER NOT NULL,
          content BLOB NOT NULL,
          vector_blob BLOB,
          FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE
      )
    )";

    *db_ << R"(
      CREATE TABLE IF NOT EXISTS task_queue (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          task_type TEXT NOT NULL,
          file_path TEXT NOT NULL,
          status TEXT DEFAULT 'PENDING',
          priority INTEGER DEFAULT 10,
          error_message TEXT,
          created_at TEXT NOT NULL,
          updated_at TEXT NOT NULL
      )
    )";

    // Create index for efficient task queue queries
    *db_ << R"(
      CREATE INDEX IF NOT EXISTS idx_task_queue_status_priority 
      ON task_queue(status, priority, created_at)
    )";
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to create tables: " + std::string(e.what()));
  }
}

/*
This upserts a file stub. If the file already exists, it will update the file.
This is used to create a file stub while the file is being processed.

@returns the id of the file
*/
int MetadataStore::upsert_file_stub(const BasicFileMetadata &basic_metadata) {
  try {
    std::string last_modified_str = time_point_to_string(basic_metadata.last_modified);
    std::string created_at_str = time_point_to_string(basic_metadata.created_at);

    // Check if file exists BEFORE doing the upsert
    int existing_id = -1;
    *db_ << "SELECT id FROM files WHERE path = ?" << basic_metadata.path >>
        [&](int id) { existing_id = id; };

    if (existing_id != -1) {
      // File exists, update it and reset AI-generated fields since file content changed
      *db_ << "UPDATE files SET original_path=?, file_hash=?, processing_status=?, "
              "tags=?, last_modified=?, file_type=?, file_size=?, "
              "summary_vector_blob=NULL, suggested_category=NULL, suggested_filename=NULL WHERE "
              "path=?"
           << basic_metadata.original_path << basic_metadata.file_hash
           << basic_metadata.processing_status << basic_metadata.tags << last_modified_str
           << to_string(basic_metadata.file_type) << static_cast<int64_t>(basic_metadata.file_size)
           << basic_metadata.path;
      return existing_id;
    } else {
      // File doesn't exist, insert new
      *db_ << "INSERT INTO files (path, original_path, file_hash, processing_status, tags, "
              "last_modified, created_at, file_type, file_size) VALUES (?,?,?,?,?,?,?,?,?)"
           << basic_metadata.path << basic_metadata.original_path << basic_metadata.file_hash
           << basic_metadata.processing_status << basic_metadata.tags << last_modified_str
           << created_at_str << to_string(basic_metadata.file_type)
           << static_cast<int64_t>(basic_metadata.file_size);
      return static_cast<int>(db_->last_insert_rowid());
    }
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to upsert file stub: " + std::string(e.what()));
  }
}

void MetadataStore::update_file_ai_analysis(int file_id,
                                            const std::vector<float> &summary_vector,
                                            const std::string &suggested_category,
                                            const std::string &suggested_filename,
                                            ProcessingStatus processing_status) {
  try {
    // Validate vector dimensions if provided
    if (!summary_vector.empty() && summary_vector.size() != VECTOR_DIMENSION) {
      throw MetadataStoreError("Vector embedding size mismatch for file_id " +
                               std::to_string(file_id) + ". Expected " +
                               std::to_string(VECTOR_DIMENSION) + " dimensions, got " +
                               std::to_string(summary_vector.size()) + ".");
    }

    // Check if file exists first
    if (!get_file_metadata(file_id).has_value()) {
      throw MetadataStoreError("File with ID " + std::to_string(file_id) + " not found");
    }

    // Convert vector to blob if not empty
    if (!summary_vector.empty()) {
      std::vector<char> vector_blob(summary_vector.size() * sizeof(float));
      std::memcpy(vector_blob.data(), summary_vector.data(), vector_blob.size());

      *db_ << "UPDATE files SET summary_vector_blob = ?, suggested_category = ?, "
              "suggested_filename = ?, processing_status = ? WHERE id = ?"
           << vector_blob << suggested_category << suggested_filename
           << to_string(processing_status) << file_id;
    } else {
      *db_ << "UPDATE files SET summary_vector_blob = NULL, suggested_category = ?, "
              "suggested_filename = ?, processing_status = ? WHERE id = ?"
           << suggested_category << suggested_filename << to_string(processing_status) << file_id;
    }
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to update file AI analysis: " + std::string(e.what()));
  }
}

void MetadataStore::upsert_chunk_metadata(int file_id, const std::vector<ProcessedChunk> &chunks) {
  if (chunks.empty())
    return;

  try {
    // Start transaction
    *db_ << "BEGIN TRANSACTION;";

    for (const auto &chunk : chunks) {
      // Convert vector to blob
      std::vector<char> vector_blob(chunk.chunk.vector_embedding.size() * sizeof(float));
      std::memcpy(vector_blob.data(), chunk.chunk.vector_embedding.data(), vector_blob.size());

      *db_ << "REPLACE INTO chunks (file_id, chunk_index, content, vector_blob) VALUES (?, ?, ?, ?)"
           << file_id << chunk.chunk.chunk_index
           << chunk.compressed_content  // Use compressed content
           << vector_blob;
    }

    *db_ << "COMMIT;";
  } catch (const sqlite::sqlite_exception &e) {
    *db_ << "ROLLBACK;";
    throw MetadataStoreError("Failed to upsert chunk metadata: " + std::string(e.what()));
  }
}

std::vector<ChunkMetadata> MetadataStore::get_chunk_metadata(std::vector<int> file_ids) {
  std::vector<ChunkMetadata> chunks;

  if (file_ids.empty())
    return chunks;

  try {
    std::string file_ids_str = int_vector_to_comma_string(file_ids);
    std::string sql = "SELECT id, file_id, chunk_index, content FROM chunks WHERE file_id IN (" +
                      file_ids_str + ") ORDER BY file_id, chunk_index";

    *db_ << sql >> [&](int id, int file_id, int chunk_index, std::vector<char> content) {
      ChunkMetadata chunk;
      chunk.id = id;
      chunk.file_id = file_id;
      chunk.chunk_index = chunk_index;
      chunk.content = std::move(content);
      chunks.push_back(std::move(chunk));
    };
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to get chunk metadata: " + std::string(e.what()));
  }

  return chunks;
}

void MetadataStore::fill_chunk_metadata(std::vector<ChunkSearchResult> &chunks) {
  if (chunks.empty()) {
    return;
  }

  try {
    std::vector<int> chunk_ids;
    chunk_ids.reserve(chunks.size());
    for (const auto &chunk : chunks) {
      chunk_ids.push_back(chunk.id);
    }

    std::string chunk_ids_str = int_vector_to_comma_string(chunk_ids);
    std::string sql =
        "SELECT id, file_id, chunk_index, content FROM chunks WHERE id IN (" + chunk_ids_str + ")";

    // Create a map to store metadata by ID
    std::unordered_map<int, std::tuple<int, int, std::vector<char>>> id_to_metadata;

    *db_ << sql >> [&](int id, int file_id, int chunk_index, std::vector<char> content) {
      id_to_metadata[id] = {file_id, chunk_index, std::move(content)};
    };

    // Fill in the metadata for each chunk (preserving distances)
    for (auto &chunk : chunks) {
      auto it = id_to_metadata.find(chunk.id);
      if (it != id_to_metadata.end()) {
        chunk.file_id = std::get<0>(it->second);
        chunk.chunk_index = std::get<1>(it->second);
        chunk.compressed_content = std::get<2>(it->second);
      } else {
        std::cout << "Chunk with ID " << chunk.id << " not found" << std::endl;
      }
    }
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to fill chunk metadata: " + std::string(e.what()));
  }
}

std::optional<FileMetadata> MetadataStore::get_file_metadata(const std::string &path) {
  try {
    std::optional<FileMetadata> result;

    *db_ << "SELECT id, path, original_path, file_hash, processing_status, tags, "
            "last_modified, created_at, file_type, file_size, summary_vector_blob, "
            "suggested_category, suggested_filename FROM files WHERE path = ?"
         << path >>
        [&](int id, std::string path, std::optional<std::string> original_path,
            std::string file_hash, std::optional<std::string> processing_status,
            std::optional<std::string> tags, std::string last_modified, std::string created_at,
            std::string file_type, int64_t file_size, std::optional<std::vector<char>> vector_blob,
            std::optional<std::string> suggested_category,
            std::optional<std::string> suggested_filename) {
          FileMetadata metadata;
          metadata.id = id;
          metadata.path = path;
          if (original_path)
            metadata.original_path = *original_path;
          metadata.file_hash = file_hash;
          if (processing_status)
            metadata.processing_status = *processing_status;
          if (tags)
            metadata.tags = *tags;
          metadata.last_modified = string_to_time_point(last_modified);
          metadata.created_at = string_to_time_point(created_at);
          metadata.file_type = file_type_from_string(file_type);
          metadata.file_size = static_cast<size_t>(file_size);

          // Handle vector blob
          if (vector_blob && vector_blob->size() == VECTOR_DIMENSION * sizeof(float)) {
            const float *data = reinterpret_cast<const float *>(vector_blob->data());
            metadata.summary_vector_embedding.assign(data, data + VECTOR_DIMENSION);
          }

          if (suggested_category)
            metadata.suggested_category = *suggested_category;
          if (suggested_filename)
            metadata.suggested_filename = *suggested_filename;

          result = std::move(metadata);
        };

    return result;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to get file metadata: " + std::string(e.what()));
  }
}

std::optional<FileMetadata> MetadataStore::get_file_metadata(int id) {
  try {
    std::optional<FileMetadata> result;

    *db_ << "SELECT id, path, original_path, file_hash, processing_status, tags, "
            "last_modified, created_at, file_type, file_size, summary_vector_blob, "
            "suggested_category, suggested_filename FROM files WHERE id = ?"
         << id >>
        [&](int id, std::string path, std::optional<std::string> original_path,
            std::string file_hash, std::optional<std::string> processing_status,
            std::optional<std::string> tags, std::string last_modified, std::string created_at,
            std::string file_type, int64_t file_size, std::optional<std::vector<char>> vector_blob,
            std::optional<std::string> suggested_category,
            std::optional<std::string> suggested_filename) {
          FileMetadata metadata;
          metadata.id = id;
          metadata.path = path;
          if (original_path)
            metadata.original_path = *original_path;
          metadata.file_hash = file_hash;
          if (processing_status)
            metadata.processing_status = *processing_status;
          if (tags)
            metadata.tags = *tags;
          metadata.last_modified = string_to_time_point(last_modified);
          metadata.created_at = string_to_time_point(created_at);
          metadata.file_type = file_type_from_string(file_type);
          metadata.file_size = static_cast<size_t>(file_size);

          // Handle vector blob
          if (vector_blob && vector_blob->size() == VECTOR_DIMENSION * sizeof(float)) {
            const float *data = reinterpret_cast<const float *>(vector_blob->data());
            metadata.summary_vector_embedding.assign(data, data + VECTOR_DIMENSION);
          }

          if (suggested_category)
            metadata.suggested_category = *suggested_category;
          if (suggested_filename)
            metadata.suggested_filename = *suggested_filename;

          result = std::move(metadata);
        };

    return result;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to get file metadata by ID: " + std::string(e.what()));
  }
}

bool MetadataStore::file_exists(const std::string &path) {
  return get_file_metadata(path).has_value();
}

// TODO: This will probably not be super usable. eventually it should have pagination
std::vector<FileMetadata> MetadataStore::list_all_files() {
  std::vector<FileMetadata> files;

  try {
    *db_ << "SELECT id, path, file_hash, last_modified, created_at, file_type, file_size, "
            "summary_vector_blob FROM files" >>
        [&](int id, std::string path, std::string file_hash, std::string last_modified,
            std::string created_at, std::string file_type, int64_t file_size,
            std::optional<std::vector<char>> vector_blob) {
          FileMetadata metadata;
          metadata.id = id;
          metadata.path = path;
          metadata.file_hash = file_hash;
          metadata.last_modified = string_to_time_point(last_modified);
          metadata.created_at = string_to_time_point(created_at);
          metadata.file_type = file_type_from_string(file_type);
          metadata.file_size = static_cast<size_t>(file_size);

          // Handle vector blob
          if (vector_blob && vector_blob->size() == VECTOR_DIMENSION * sizeof(float)) {
            const float *data = reinterpret_cast<const float *>(vector_blob->data());
            metadata.summary_vector_embedding.assign(data, data + VECTOR_DIMENSION);
          }

          files.push_back(std::move(metadata));
        };
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to list all files: " + std::string(e.what()));
  }

  return files;
}

void MetadataStore::delete_file_metadata(const std::string &path) {
  try {
    *db_ << "DELETE FROM files WHERE path = ?" << path;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to delete file metadata: " + std::string(e.what()));
  }
}

void MetadataStore::rebuild_faiss_index() {
  // Clean up existing index
  if (faiss_index_) {
    delete faiss_index_;
    faiss_index_ = nullptr;
  }

  // Create new index
  faiss_index_ = create_base_index();
  if (!faiss_index_) {
    throw MetadataStoreError("Failed to create base Faiss index");
  }

  try {
    std::vector<faiss::idx_t> faiss_ids;
    std::vector<float> all_vectors_flat;
    int current_num_vectors = 0;

    // Fetch all IDs and vectors from the database
    *db_ << "SELECT id, summary_vector_blob FROM files WHERE summary_vector_blob IS NOT NULL" >>
        [&](int64_t id, std::vector<char> vector_blob) {
          // Validate vector size to match expected dimension
          if (vector_blob.size() == VECTOR_DIMENSION * sizeof(float)) {
            faiss_ids.push_back(id);
            const float *vec_ptr = reinterpret_cast<const float *>(vector_blob.data());
            // Append vector data to a flat vector
            all_vectors_flat.insert(all_vectors_flat.end(), vec_ptr, vec_ptr + VECTOR_DIMENSION);
            current_num_vectors++;
          } else if (vector_blob.size() > 0) {
            // Log a warning for mismatched dimensions, but continue processing others
            std::cerr << "Warning: Skipping file ID " << id
                      << " during index rebuild due to mismatched vector dimension. Expected "
                      << VECTOR_DIMENSION * sizeof(float) << " bytes, got " << vector_blob.size()
                      << " bytes." << std::endl;
          }
        };

    if (current_num_vectors > 0) {
      // Add all vectors to the Faiss index in one go
      faiss_index_->add_with_ids(current_num_vectors, all_vectors_flat.data(), faiss_ids.data());
    }
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to rebuild Faiss index: " + std::string(e.what()));
  }
}

std::vector<FileSearchResult> MetadataStore::search_similar_files(
    const std::vector<float> &query_vector, int k) {
  if (!faiss_index_) {
    return {};
  }

  int actual_k = std::min(k, (int)faiss_index_->ntotal);
  if (actual_k <= 0) {
    return {};
  }

  std::vector<float> distances(actual_k);
  std::vector<faiss::idx_t> labels(actual_k);
  search_faiss_index(faiss_index_, query_vector, actual_k, distances, labels);

  std::vector<FileSearchResult> results;
  for (int i = 0; i < actual_k; ++i) {
    long faiss_id = labels[i];
    // Faiss returns -1 for padded results if it finds fewer than 'k' neighbors
    if (faiss_id == -1) {
      continue;
    }

    // Fetch full metadata for the file using its ID from SQLite
    std::optional<FileMetadata> metadata = get_file_metadata(static_cast<int>(faiss_id));

    if (metadata) {
      results.push_back({metadata->id, distances[i], std::move(*metadata)});
    } else {
      // This is a critical warning: Faiss returned an ID, but it's not in the DB.
      std::cerr << "Warning: Faiss returned ID " << faiss_id
                << " but no corresponding metadata found in DB." << std::endl;
    }
  }

  return results;
}

std::vector<ChunkSearchResult> MetadataStore::search_similar_chunks(
    const std::vector<int> &file_ids, const std::vector<float> &query_vector, int k) {
  // Early return if no file IDs provided
  if (file_ids.empty()) {
    return {};
  }

  try {
    // Create temporary chunk index
    auto chunk_index = std::unique_ptr<faiss::IndexIDMap>(create_base_index());
    std::vector<faiss::idx_t> faiss_ids;
    std::vector<float> all_vectors_flat;
    int current_num_vectors = 0;

    std::string file_ids_str = int_vector_to_comma_string(file_ids);
    std::string sql = "SELECT id, vector_blob FROM chunks WHERE file_id IN (" + file_ids_str +
                      ") ORDER BY file_id, chunk_index";

    *db_ << sql >> [&](int64_t id, std::vector<char> vector_blob) {
      // Validate vector size to match expected dimension
      if (vector_blob.size() == VECTOR_DIMENSION * sizeof(float)) {
        faiss_ids.push_back(id);
        const float *vec_ptr = reinterpret_cast<const float *>(vector_blob.data());
        // Append vector data to a flat vector
        all_vectors_flat.insert(all_vectors_flat.end(), vec_ptr, vec_ptr + VECTOR_DIMENSION);
        current_num_vectors++;
      } else if (vector_blob.size() > 0) {
        // Log a warning for mismatched dimensions, but continue processing others
        std::cerr << "Warning: Skipping chunk ID " << id
                  << " during index rebuild due to mismatched vector dimension. Expected "
                  << VECTOR_DIMENSION * sizeof(float) << " bytes, got " << vector_blob.size()
                  << " bytes." << std::endl;
      }
    };

    // Build a faiss index of the chunks by batch adding at the end
    if (current_num_vectors > 0) {
      chunk_index->add_with_ids(current_num_vectors, all_vectors_flat.data(), faiss_ids.data());
    }

    // Search the faiss index for the query vector
    int actual_k = std::min(k, (int)chunk_index->ntotal);
    if (actual_k <= 0) {
      return {};
    }

    std::vector<float> distances(actual_k);
    std::vector<faiss::idx_t> labels(actual_k);
    search_faiss_index(chunk_index.get(), query_vector, actual_k, distances, labels);

    // The labels are chunk ids, so we need to get the actual chunks from the database
    std::vector<ChunkSearchResult> chunks;
    chunks.reserve(labels.size());
    for (int i = 0; i < labels.size(); i++) {
      const auto &faiss_id = labels[i];
      ChunkSearchResult chunk;
      chunk.id = static_cast<int>(faiss_id);
      chunk.distance = distances[i];
      chunks.push_back(chunk);
    }

    // Fill in the metadata (file_id, content)
    fill_chunk_metadata(chunks);
    return chunks;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to search similar chunks: " + std::string(e.what()));
  }
}

/* Wrapper for faiss search with error handling and dimension checking */
void MetadataStore::search_faiss_index(faiss::IndexIDMap *index,
                                       const std::vector<float> &query_vector,
                                       int k,
                                       std::vector<float> &distances,
                                       std::vector<faiss::idx_t> &labels) {
  if (!index || index->ntotal == 0) {
    // If the index is empty or not built, we can't search.
    throw MetadataStoreError("Faiss index not initialized or empty. Cannot perform search.");
  }

  if (query_vector.size() != VECTOR_DIMENSION) {
    throw MetadataStoreError("Query vector dimension mismatch. Expected " +
                             std::to_string(VECTOR_DIMENSION) + ", got " +
                             std::to_string(query_vector.size()));
  }

  index->search(1, query_vector.data(), k, distances.data(), labels.data());
}

faiss::IndexIDMap *MetadataStore::create_base_index() {
  auto base_index = new faiss::IndexHNSWFlat(VECTOR_DIMENSION, HNSW_M_PARAM);
  base_index->hnsw.efConstruction = HNSW_EF_CONSTRUCTION_PARAM;
  // Wrap with IDMap to enable add_with_ids
  return new faiss::IndexIDMap(base_index);
}

std::string MetadataStore::int_vector_to_comma_string(const std::vector<int> &vector) {
  std::stringstream ss;
  for (size_t i = 0; i < vector.size(); ++i) {
    ss << vector[i];
    if (i < vector.size() - 1)
      ss << ",";
  }
  return ss.str();
}

// Task queue management functions

long long MetadataStore::create_task(const std::string& task_type,
                                     const std::string& file_path,
                                     int priority) {
  try {
    auto now = std::chrono::system_clock::now();
    std::string created_at_str = time_point_to_string(now);
    std::string updated_at_str = created_at_str;

    *db_ << "INSERT INTO task_queue (task_type, file_path, priority, created_at, updated_at) VALUES (?,?,?,?,?)"
         << task_type << file_path << priority << created_at_str << updated_at_str;
    
    return static_cast<long long>(db_->last_insert_rowid());
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to create task: " + std::string(e.what()));
  }
}

std::optional<Task> MetadataStore::fetch_and_claim_next_task() {
  try {
    std::optional<Task> result;
    
    // Start transaction to ensure atomic fetch and update
    *db_ << "BEGIN TRANSACTION;";
    
    // Find the highest priority pending task
    std::string pending_status = to_string(TaskStatus::PENDING);
    *db_ << "SELECT id, task_type, file_path, status, priority, error_message, created_at, updated_at "
            "FROM task_queue WHERE status = ? ORDER BY priority ASC, created_at ASC LIMIT 1"
        << pending_status >>
        [&](long long id, std::string task_type, std::string file_path, std::string status_db, 
            int priority, std::optional<std::string> error_message, 
            std::string created_at, std::string updated_at) {
          Task task;
          task.id = id;
          task.task_type = task_type;
          task.file_path = file_path;
          task.status = task_status_from_string(status_db);
          task.priority = priority;
          if (error_message) {
            task.error_message = *error_message;
          }
          task.created_at = string_to_time_point(created_at);
          task.updated_at = string_to_time_point(updated_at);
          result = std::move(task);
        };

    if (result) {
      // Mark task as PROCESSING
      auto now = std::chrono::system_clock::now();
      std::string updated_at_str = time_point_to_string(now);
      std::string processing_status = to_string(TaskStatus::PROCESSING);
      
      *db_ << "UPDATE task_queue SET status = ?, updated_at = ? WHERE id = ?"
           << processing_status << updated_at_str << result->id;
      
      result->status = TaskStatus::PROCESSING;
      result->updated_at = now;
    }
    
    *db_ << "COMMIT;";
    return result;
  } catch (const sqlite::sqlite_exception &e) {
    *db_ << "ROLLBACK;";
    throw MetadataStoreError("Failed to fetch and claim next task: " + std::string(e.what()));
  }
}

void MetadataStore::update_task_status(long long task_id, TaskStatus new_status) {
  try {
    auto now = std::chrono::system_clock::now();
    std::string updated_at_str = time_point_to_string(now);
    std::string status_str = to_string(new_status);
    
    *db_ << "UPDATE task_queue SET status = ?, updated_at = ? WHERE id = ?"
         << status_str << updated_at_str << task_id;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to update task status: " + std::string(e.what()));
  }
}

void MetadataStore::mark_task_as_failed(long long task_id, const std::string& error_message) {
  try {
    auto now = std::chrono::system_clock::now();
    std::string updated_at_str = time_point_to_string(now);
    std::string failed_status = to_string(TaskStatus::FAILED);
    
    *db_ << "UPDATE task_queue SET status = ?, error_message = ?, updated_at = ? WHERE id = ?"
         << failed_status << error_message << updated_at_str << task_id;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to mark task as failed: " + std::string(e.what()));
  }
}

std::vector<Task> MetadataStore::get_tasks_by_status(TaskStatus status) {
  std::vector<Task> tasks;
  std::string status_str = to_string(status);
  
  try {
    *db_ << "SELECT id, task_type, file_path, status, priority, error_message, created_at, updated_at "
            "FROM task_queue WHERE status = ? ORDER BY priority ASC, created_at ASC"
         << status_str >>
        [&](long long id, std::string task_type, std::string file_path, std::string status_db, 
            int priority, std::optional<std::string> error_message, 
            std::string created_at, std::string updated_at) {
          Task task;
          task.id = id;
          task.task_type = task_type;
          task.file_path = file_path;
          task.status = task_status_from_string(status_db);
          task.priority = priority;
          if (error_message) {
            task.error_message = *error_message;
          }
          task.created_at = string_to_time_point(created_at);
          task.updated_at = string_to_time_point(updated_at);
          tasks.push_back(std::move(task));
        };
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to get tasks by status: " + std::string(e.what()));
  }
  
  return tasks;
}

void MetadataStore::clear_completed_tasks(int older_than_days) {
  try {
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * older_than_days);
    std::string cutoff_str = time_point_to_string(cutoff_time);
    std::string completed_status = to_string(TaskStatus::COMPLETED);
    std::string failed_status = to_string(TaskStatus::FAILED);
    
    *db_ << "DELETE FROM task_queue WHERE status IN (?, ?) AND updated_at <= ?"
         << completed_status << failed_status << cutoff_str;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError("Failed to clear completed tasks: " + std::string(e.what()));
  }
}

}  // namespace magic_core