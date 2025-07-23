#include "magic_core/db/metadata_store.hpp"
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace magic_core {

// Helper for time point conversions (now explicitly defined)
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

MetadataStore::MetadataStore(const std::filesystem::path &db_path)
    : db_path_(db_path), db_(nullptr), faiss_index_(nullptr) {
  initialize();
}

MetadataStore::~MetadataStore() {
  if (faiss_index_) {
    // Clean up faiss index in memory
    delete faiss_index_;
  }
  if (db_) {
    sqlite3_close(db_);
  }
}

MetadataStore::MetadataStore(MetadataStore &&other) noexcept
    : db_path_(std::move(other.db_path_)), db_(other.db_), faiss_index_(other.faiss_index_) {
  other.db_ = nullptr;
  other.faiss_index_ = nullptr;
}

MetadataStore &MetadataStore::operator=(MetadataStore &&other) noexcept {
  if (this != &other) {
    if (faiss_index_) {
      // Clean up current faiss index in memory
      delete faiss_index_;
    }
    if (db_) {
      sqlite3_close(db_);
    }
    db_path_ = std::move(other.db_path_);
    db_ = other.db_;
    faiss_index_ = other.faiss_index_;
    other.db_ = nullptr;
    other.faiss_index_ = nullptr;
  }
  return *this;
}

void MetadataStore::initialize() {
  // Create database directory if it doesn't exist
  std::filesystem::create_directories(db_path_.parent_path());

  // Open database
  int rc = sqlite3_open(db_path_.c_str(), &db_);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
  }

  create_tables();
  // Always need to build the faiss index on startup
  rebuild_faiss_index();
}

void MetadataStore::create_tables() {
  const char *sql = R"(
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
    );

    CREATE TABLE IF NOT EXISTS chunks (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        file_id INTEGER NOT NULL,
        chunk_index INTEGER NOT NULL,
        content TEXT NOT NULL,
        vector_blob BLOB,
        FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE
    );
  )";

  execute_sql(sql);
}

void MetadataStore::execute_sql(const std::string &sql) {
  char *err_msg = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = "SQL execution failed: " + std::string(err_msg);
    sqlite3_free(err_msg);
    throw MetadataStoreError(error);
  }
}

std::chrono::system_clock::time_point MetadataStore::get_file_last_modified(
    const std::filesystem::path &file_path) {
  auto ftime = std::filesystem::last_write_time(file_path);
  // Convert file_time_type to system_clock::time_point
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
  return sctp;
}

int MetadataStore::create_file_stub(const BasicFileMetadata &basic_metadata) {
  std::string last_modified_str = time_point_to_string(basic_metadata.last_modified);
  std::string created_at_str = time_point_to_string(basic_metadata.created_at);
  
  const char *sql = 
      "INSERT INTO files (path, original_path, file_hash, processing_status, tags, "
      "last_modified, created_at, file_type, file_size) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_bind_text(stmt, 1, basic_metadata.path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, basic_metadata.original_path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, basic_metadata.file_hash.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, basic_metadata.processing_status.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, basic_metadata.tags.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 6, last_modified_str.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, created_at_str.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 8, to_string(basic_metadata.file_type).c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 9, basic_metadata.file_size);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw MetadataStoreError("Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_finalize(stmt);
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void MetadataStore::update_file_ai_analysis(int file_id, 
                                           const std::vector<float> &summary_vector,
                                           const std::string &suggested_category,
                                           const std::string &suggested_filename) {
  const char *sql = 
      "UPDATE files SET summary_vector_blob = ?, suggested_category = ?, suggested_filename = ? "
      "WHERE id = ?";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
  }

  // Bind vector as BLOB
  if (!summary_vector.empty()) {
    if (summary_vector.size() != VECTOR_DIMENSION) {
      throw MetadataStoreError("Vector embedding size mismatch for file_id " + std::to_string(file_id) +
                               ". Expected " + std::to_string(VECTOR_DIMENSION) +
                               " dimensions, got " + std::to_string(summary_vector.size()) + ".");
    }
    sqlite3_bind_blob(stmt, 1, summary_vector.data(),
                      summary_vector.size() * sizeof(float), SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 1);
  }

  sqlite3_bind_text(stmt, 2, suggested_category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, suggested_filename.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, file_id);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw MetadataStoreError("Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
  }

  // Check if any rows were actually updated
  if (sqlite3_changes(db_) == 0) {
    sqlite3_finalize(stmt);
    throw MetadataStoreError("File with ID " + std::to_string(file_id) + " not found");
  }

  sqlite3_finalize(stmt);
}

void MetadataStore::upsert_chunk_metadata(
        int file_id,
        const std::vector<ChunkWithEmbedding>& chunks)
{
    if (chunks.empty()) return;

    const char* sql =
        "REPLACE INTO chunks (file_id, chunk_index, content, vector_blob) "
        "VALUES (?, ?, ?, ?)";

    // Start explicit transaction to batch insert chunks
    char* errmsg = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errmsg) != SQLITE_OK)
        throw MetadataStoreError(errmsg);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw MetadataStoreError("prepare failed: " + std::string(sqlite3_errmsg(db_)));
    }

    for (const auto& ce : chunks) {
        sqlite3_bind_int (stmt, 1, file_id);
        sqlite3_bind_int (stmt, 2, ce.chunk.chunk_index);
        sqlite3_bind_text(stmt, 3, ce.chunk.content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 4,
                          ce.embedding.data(),
                          static_cast<int>(ce.embedding.size() * sizeof(float)),
                          SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw MetadataStoreError("step failed: " + std::string(sqlite3_errmsg(db_)));
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);

    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errmsg) != SQLITE_OK)
        throw MetadataStoreError(errmsg);
}

std::optional<FileMetadata> MetadataStore::get_file_metadata(const std::string &path) {
  std::string sql =
      "SELECT id, path, original_path, file_hash, processing_status, tags, "
      "last_modified, created_at, file_type, file_size, summary_vector_blob, "
      "suggested_category, suggested_filename "
      "FROM files WHERE path = ?";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    FileMetadata metadata;
    metadata.id = sqlite3_column_int(stmt, 0);
    metadata.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    
    const char* original_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    if (original_path) metadata.original_path = original_path;
    
    metadata.file_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    
    const char* processing_status = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    if (processing_status) metadata.processing_status = processing_status;
    
    const char* tags = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    if (tags) metadata.tags = tags;
    
    metadata.last_modified =
        string_to_time_point(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6)));
    metadata.created_at =
        string_to_time_point(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7)));
    metadata.file_type =
        file_type_from_string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8)));
    metadata.file_size = sqlite3_column_int64(stmt, 9);

    const void *vector_blob = sqlite3_column_blob(stmt, 10);
    int blob_size = sqlite3_column_bytes(stmt, 10);
    if (vector_blob && blob_size > 0) {
      if (blob_size % sizeof(float) != 0 || blob_size / sizeof(float) != VECTOR_DIMENSION) {
        std::cerr << "Warning: Vector for ID " << metadata.id << " has unexpected size. Expected "
                  << VECTOR_DIMENSION << " floats, got " << blob_size / sizeof(float) << "."
                  << std::endl;
      }
      metadata.summary_vector_embedding.assign(
          reinterpret_cast<const float *>(vector_blob),
          reinterpret_cast<const float *>(vector_blob) + (blob_size / sizeof(float)));
    }

    const char* suggested_category = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
    if (suggested_category) metadata.suggested_category = suggested_category;
    
    const char* suggested_filename = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 12));
    if (suggested_filename) metadata.suggested_filename = suggested_filename;

    sqlite3_finalize(stmt);
    return metadata;
  }

  sqlite3_finalize(stmt);
  return std::nullopt;
}

std::optional<FileMetadata> MetadataStore::get_file_metadata(int id) {
  std::string sql =
      "SELECT id, path, original_path, file_hash, processing_status, tags, "
      "last_modified, created_at, file_type, file_size, summary_vector_blob, "
      "suggested_category, suggested_filename "
      "FROM files WHERE id = ?";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_bind_int(stmt, 1, id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    FileMetadata metadata;
    metadata.id = sqlite3_column_int(stmt, 0);
    metadata.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    
    const char* original_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    if (original_path) metadata.original_path = original_path;
    
    metadata.file_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    
    const char* processing_status = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    if (processing_status) metadata.processing_status = processing_status;
    
    const char* tags = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    if (tags) metadata.tags = tags;
    
    metadata.last_modified =
        string_to_time_point(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6)));
    metadata.created_at =
        string_to_time_point(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7)));
    metadata.file_type =
        file_type_from_string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8)));
    metadata.file_size = sqlite3_column_int64(stmt, 9);

    const void *vector_blob = sqlite3_column_blob(stmt, 10);
    int blob_size = sqlite3_column_bytes(stmt, 10);
    if (vector_blob && blob_size > 0) {
      if (blob_size % sizeof(float) != 0 || blob_size / sizeof(float) != VECTOR_DIMENSION) {
        std::cerr << "Warning: Vector for ID " << metadata.id << " has unexpected size. Expected "
                  << VECTOR_DIMENSION << " floats, got " << blob_size / sizeof(float) << "."
                  << std::endl;
      }
      metadata.summary_vector_embedding.assign(
          reinterpret_cast<const float *>(vector_blob),
          reinterpret_cast<const float *>(vector_blob) + (blob_size / sizeof(float)));
    }

    const char* suggested_category = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
    if (suggested_category) metadata.suggested_category = suggested_category;
    
    const char* suggested_filename = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 12));
    if (suggested_filename) metadata.suggested_filename = suggested_filename;

    sqlite3_finalize(stmt);
    return metadata;
  }

  sqlite3_finalize(stmt);
  return std::nullopt;
}

bool MetadataStore::file_exists(const std::string &path) {
  return get_file_metadata(path).has_value();
}

// TODO: This will probably not be super usable. eventually it should have pagination
std::vector<FileMetadata> MetadataStore::list_all_files() {
  std::vector<FileMetadata> files;
  std::string sql =
      "SELECT id, path, file_hash, last_modified, created_at, file_type, file_size, summary_vector_blob FROM "
      "files";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    FileMetadata metadata;
    metadata.id = sqlite3_column_int(stmt, 0);
    metadata.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    metadata.file_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    metadata.last_modified =
        string_to_time_point(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
    metadata.created_at =
        string_to_time_point(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
    metadata.file_type =
        file_type_from_string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5)));
    metadata.file_size = sqlite3_column_int64(stmt, 6);

    const void *vector_blob = sqlite3_column_blob(stmt, 7);
    int blob_size = sqlite3_column_bytes(stmt, 7);
    if (vector_blob && blob_size > 0) {
      if (blob_size % sizeof(float) != 0 || blob_size / sizeof(float) != VECTOR_DIMENSION) {
        std::cerr << "Warning: Vector for ID " << metadata.id << " has unexpected size. Expected "
                  << VECTOR_DIMENSION << " floats, got " << blob_size / sizeof(float) << "."
                  << std::endl;
      }
      metadata.summary_vector_embedding.assign(
          reinterpret_cast<const float *>(vector_blob),
          reinterpret_cast<const float *>(vector_blob) + (blob_size / sizeof(float)));
    }

    files.push_back(metadata);
  }

  sqlite3_finalize(stmt);
  return files;
}

void MetadataStore::delete_file_metadata(const std::string &path) {
  std::string sql = "DELETE FROM files WHERE path = ?";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw MetadataStoreError("Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_finalize(stmt);
}

void MetadataStore::rebuild_faiss_index() {
  if (faiss_index_) {
    delete faiss_index_;
    faiss_index_ = nullptr;
  }

  // Create HNSW index wrapped with IDMap for custom ID support
  auto base_index = new faiss::IndexHNSWFlat(VECTOR_DIMENSION, HNSW_M_PARAM);
  base_index->hnsw.efConstruction = HNSW_EF_CONSTRUCTION_PARAM;
  
  // Wrap with IDMap to enable add_with_ids
  faiss_index_ = new faiss::IndexIDMap(base_index);
  
  std::vector<faiss::idx_t> faiss_ids;
  std::vector<float> all_vectors_flat;
  int current_num_vectors = 0;

  // Fetch all IDs and vectors from the database
  std::string sql = "SELECT id, summary_vector_blob FROM files WHERE summary_vector_blob IS NOT NULL";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw MetadataStoreError("Failed to prepare statement for index rebuild: " +
                             std::string(sqlite3_errmsg(db_)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    long id = sqlite3_column_int64(stmt, 0);  // SQLite ID (maps to Faiss ID)
    const void *vector_blob = sqlite3_column_blob(stmt, 1);
    int blob_size = sqlite3_column_bytes(stmt, 1);

    // Validate vector size to match expected dimension
    if (vector_blob && blob_size == VECTOR_DIMENSION * sizeof(float)) {
      faiss_ids.push_back(id);
      const float *vec_ptr = reinterpret_cast<const float *>(vector_blob);
      // Append vector data to a flat vector
      all_vectors_flat.insert(all_vectors_flat.end(), vec_ptr, vec_ptr + VECTOR_DIMENSION);
      current_num_vectors++;
    } else if (vector_blob && blob_size > 0) {
      // Log a warning for mismatched dimensions, but continue processing others
      std::cerr << "Warning: Skipping file ID " << id
                << " during index rebuild due to mismatched vector dimension. Expected "
                << VECTOR_DIMENSION * sizeof(float) << " bytes, got " << blob_size << " bytes."
                << std::endl;
    }
    // If vector_blob is null or size is 0, it means no vector was stored, so skip
  }
  sqlite3_finalize(stmt);

  if (current_num_vectors > 0) {
    // Add all vectors to the Faiss index in one go
    faiss_index_->add_with_ids(current_num_vectors, all_vectors_flat.data(), faiss_ids.data());
    std::cout << "Faiss index rebuilt with " << current_num_vectors << " vectors." << std::endl;
  } else {
    std::cout << "Faiss index rebuilt, no vectors found to add." << std::endl;
  }
}

std::vector<SearchResult> MetadataStore::search_similar_files(
    const std::vector<float> &query_vector, int k) {
  if (!faiss_index_ || faiss_index_->ntotal == 0) {
    // If the index is empty or not built, we can't search.
    throw MetadataStoreError("Faiss index not initialized or empty. Cannot perform search.");
  }

  if (query_vector.size() != VECTOR_DIMENSION) {
    throw MetadataStoreError("Query vector dimension mismatch - HUH?. Expected " +
                             std::to_string(VECTOR_DIMENSION) + ", got " +
                             std::to_string(query_vector.size()));
  }

  // Ensure k is not greater than the number of vectors in the index
  // Need to decide whether this actually matters for us
  int actual_k = std::min(k, (int)faiss_index_->ntotal);
  if (actual_k <= 0) {
    return {};  // No results if k is non-positive or index is empty
  }

  std::vector<float> distances(actual_k);
  std::vector<faiss::idx_t> labels(actual_k);  // IDs returned by Faiss

  // Perform the search
  // The first argument '1' means we are searching with one query vector
  faiss_index_->search(1, query_vector.data(), actual_k, distances.data(), labels.data());

  std::vector<SearchResult> results;
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
      // This should ideally not happen if IDs are consistent between Faiss and SQLite,
      // which they should be with the rebuild-on-startup strategy.
      std::cerr << "Warning: Faiss returned ID " << faiss_id
                << " but no corresponding metadata found in DB." << std::endl;
    }
  }

  return results;
}
}  // namespace magic_core