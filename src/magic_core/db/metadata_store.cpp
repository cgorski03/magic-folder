#include "magic_core/db/metadata_store.hpp"

#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "magic_core/db/pooled_connection.hpp"
#include "magic_core/db/sqlite_error_utils.hpp"
#include "magic_core/db/transaction.hpp"

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

MetadataStore::MetadataStore(DatabaseManager &db_manager)
    : db_manager_(db_manager), faiss_index_(nullptr) {
  rebuild_faiss_index();
}
MetadataStore::~MetadataStore() {
  if (faiss_index_) {
    delete faiss_index_;
  }
}

// MetadataStore is non-movable to keep DB references stable

void MetadataStore::initialize() {
  rebuild_faiss_index();
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

    int result_id = -1;
    PooledConnection conn(db_manager_);
    Transaction tx(*conn, /*immediate*/ true);

    // Check if file exists BEFORE doing the upsert
    int existing_id = -1;
    *conn << "SELECT id FROM files WHERE path = ?" << basic_metadata.path >>
        [&](int id) { existing_id = id; };

    if (existing_id != -1) {
      // File exists, update it and reset AI-generated fields since file content changed
      *conn << "UPDATE files SET original_path=?, file_hash=?, processing_status=?, "
               "tags=?, last_modified=?, file_type=?, file_size=?, "
               "summary_vector_blob=NULL, suggested_category=NULL, suggested_filename=NULL WHERE "
               "path=?"
            << basic_metadata.original_path << basic_metadata.content_hash
            << to_string(basic_metadata.processing_status) << basic_metadata.tags
            << last_modified_str << to_string(basic_metadata.file_type)
            << static_cast<int64_t>(basic_metadata.file_size) << basic_metadata.path;
      result_id = existing_id;
    } else {
      // File doesn't exist, insert new
      *conn << "INSERT INTO files (path, original_path, file_hash, processing_status, tags, "
               "last_modified, created_at, file_type, file_size) VALUES (?,?,?,?,?,?,?,?,?)"
            << basic_metadata.path << basic_metadata.original_path << basic_metadata.content_hash
            << to_string(basic_metadata.processing_status) << basic_metadata.tags
            << last_modified_str << created_at_str << to_string(basic_metadata.file_type)
            << static_cast<int64_t>(basic_metadata.file_size);
      result_id = static_cast<int>(conn->last_insert_rowid());
    }

    tx.commit();
    return result_id;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("upsert_file_stub", e));
  }
}

void MetadataStore::update_file_ai_analysis(int file_id,
                                            const std::vector<float> &summary_vector,
                                            const std::string &suggested_category,
                                            const std::string &suggested_filename,
                                            ProcessingStatus processing_status) {
  try {
    PooledConnection conn(db_manager_);
    Transaction tx(*conn, true);

    // Validate vector dimensions if provided
    if (!summary_vector.empty() && summary_vector.size() != VECTOR_DIMENSION) {
      throw MetadataStoreError("Vector embedding size mismatch for file_id " +
                               std::to_string(file_id) + ". Expected " +
                               std::to_string(VECTOR_DIMENSION) + " dimensions, got " +
                               std::to_string(summary_vector.size()) + ".");
    }

    bool exists = false;
    *conn << "SELECT 1 FROM files WHERE id = ? LIMIT 1" << file_id >> [&](int /*dummy*/) {
      exists = true;
    };
    if (!exists) {
      throw MetadataStoreError("File with ID " + std::to_string(file_id) + " not found");
    }

    // Convert vector to blob if not empty
    if (!summary_vector.empty()) {
      std::vector<char> vector_blob(summary_vector.size() * sizeof(float));
      std::memcpy(vector_blob.data(), summary_vector.data(), vector_blob.size());

      *conn << "UPDATE files SET summary_vector_blob = ?, suggested_category = ?, "
               "suggested_filename = ?, processing_status = ? WHERE id = ?"
            << vector_blob << suggested_category << suggested_filename
            << to_string(processing_status) << file_id;
    } else {
      *conn << "UPDATE files SET summary_vector_blob = NULL, suggested_category = ?, "
               "suggested_filename = ?, processing_status = ? WHERE id = ?"
            << suggested_category << suggested_filename << to_string(processing_status) << file_id;
    }
    tx.commit();
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("update_file_ai_analysis", e));
  }
}

void MetadataStore::update_file_processing_status(int file_id, ProcessingStatus processing_status) {
  try {
    PooledConnection conn(db_manager_);
    *conn << "UPDATE files SET processing_status = ? WHERE id = ?" << to_string(processing_status)
          << file_id;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("update_file_processing_status", e));
  }
}

void MetadataStore::upsert_chunk_metadata(int file_id, const std::vector<ProcessedChunk> &chunks) {
  if (chunks.empty())
    return;

  try {
    PooledConnection conn(db_manager_);
    Transaction tx(*conn, true);
    for (const auto &chunk : chunks) {
      std::vector<char> vector_blob(chunk.chunk.vector_embedding.size() * sizeof(float));
      std::memcpy(vector_blob.data(), chunk.chunk.vector_embedding.data(), vector_blob.size());
      *conn
          << "REPLACE INTO chunks (file_id, chunk_index, content, vector_blob) VALUES (?, ?, ?, ?)"
          << file_id << chunk.chunk.chunk_index << chunk.compressed_content << vector_blob;
    }
    tx.commit();
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("upsert_chunk_metadata", e));
  }
}

std::vector<ChunkMetadata> MetadataStore::get_chunk_metadata(std::vector<int> file_ids) {
  std::vector<ChunkMetadata> chunks;

  if (file_ids.empty())
    return chunks;

  try {
    PooledConnection conn(db_manager_);

    std::string file_ids_str = int_vector_to_comma_string(file_ids);

    *conn << "SELECT id, file_id, chunk_index, content FROM chunks WHERE file_id IN (" +
                 file_ids_str + ") ORDER BY file_id, chunk_index" >>
        [&](int id, int file_id, int chunk_index, std::vector<char> content) {
          ChunkMetadata chunk;
          chunk.id = id;
          chunk.file_id = file_id;
          chunk.chunk_index = chunk_index;
          chunk.content = std::move(content);
          chunks.push_back(std::move(chunk));
        };
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("get_chunk_metadata", e));
  }

  return chunks;
}

void MetadataStore::fill_chunk_metadata(std::vector<ChunkSearchResult> &chunks) {
  if (chunks.empty()) {
    return;
  }

  try {
    PooledConnection conn(db_manager_);
    std::vector<int> chunk_ids;
    chunk_ids.reserve(chunks.size());
    for (const auto &chunk : chunks) {
      chunk_ids.push_back(chunk.id);
    }

    std::string chunk_ids_str = int_vector_to_comma_string(chunk_ids);
    std::unordered_map<int, std::tuple<int, int, std::vector<char>>> id_to_metadata;

    *conn << "SELECT id, file_id, chunk_index, content FROM chunks WHERE id IN (" + chunk_ids_str +
                 ")" >>
        [&](int id, int file_id, int chunk_index, std::vector<char> content) {
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
    throw MetadataStoreError(format_db_error("fill_chunk_metadata", e));
  }
}

std::optional<FileMetadata> MetadataStore::get_file_metadata(const std::string &path) {
  try {
    std::optional<FileMetadata> result;
    PooledConnection conn(db_manager_);

    *conn << "SELECT id, path, original_path, file_hash, processing_status, tags, "
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
          metadata.content_hash = file_hash;
          if (processing_status)
            metadata.processing_status = processing_status_from_string(*processing_status);
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
    throw MetadataStoreError(format_db_error("get_file_metadata_by_path", e));
  }
}

std::optional<FileMetadata> MetadataStore::get_file_metadata(int id) {
  try {
    std::optional<FileMetadata> result;
    PooledConnection conn(db_manager_);
    *conn << "SELECT id, path, original_path, file_hash, processing_status, tags, "
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
          metadata.content_hash = file_hash;
          if (processing_status)
            metadata.processing_status = processing_status_from_string(*processing_status);
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
    throw MetadataStoreError(format_db_error("get_file_metadata_by_id", e));
  }
}

bool MetadataStore::file_exists(const std::string &path) {
  return get_file_metadata(path).has_value();
}

std::optional<ProcessingStatus> MetadataStore::file_processing_status(std::string content_hash) {
  try {
    PooledConnection conn(db_manager_);
    std::optional<ProcessingStatus> result;
    // Column is stored as file_hash in the schema
    *conn << "SELECT processing_status FROM files WHERE file_hash = ?" << content_hash >>
        [&](std::string processing_status) {
          result = processing_status_from_string(processing_status);
        };
    return result;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("file_processing_status", e));
  }
}

// TODO: This will probably not be super usable. eventually it should have pagination
std::vector<FileMetadata> MetadataStore::list_all_files() {
  std::vector<FileMetadata> files;

  try {
    PooledConnection conn(db_manager_);
    *conn << "SELECT id, path, file_hash, last_modified, created_at, file_type, file_size, "
             "summary_vector_blob FROM files" >>
        [&](int id, std::string path, std::string file_hash, std::string last_modified,
            std::string created_at, std::string file_type, int64_t file_size,
            std::optional<std::vector<char>> vector_blob) {
          FileMetadata metadata;
          metadata.id = id;
          metadata.path = path;
          metadata.content_hash = file_hash;
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
    throw MetadataStoreError(format_db_error("list_all_files", e));
  }

  return files;
}

void MetadataStore::delete_file_metadata(const std::string &path) {
  try {
    PooledConnection conn(db_manager_);
    *conn << "DELETE FROM files WHERE path = ?" << path;
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("delete_file_metadata", e));
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

    {
      // Scope the connection strictly to the DB fetch
      PooledConnection conn(db_manager_);
      // Fetch all IDs and vectors from the database
      *conn << "SELECT id, summary_vector_blob FROM files WHERE summary_vector_blob IS NOT NULL" >>
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
    }

    if (current_num_vectors > 0) {
      // Add all vectors to the Faiss index in one go
      faiss_index_->add_with_ids(current_num_vectors, all_vectors_flat.data(), faiss_ids.data());
    }
  } catch (const sqlite::sqlite_exception &e) {
    throw MetadataStoreError(format_db_error("rebuild_faiss_index", e));
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

  // Build a list of valid ids in label order
  std::vector<int> label_ids;
  label_ids.reserve(actual_k);
  for (int i = 0; i < actual_k; ++i) {
    if (labels[i] != -1) {
      label_ids.push_back(static_cast<int>(labels[i]));
    }
  }
  if (label_ids.empty()) {
    return {};
  }

  // Fetch all required file metadata in one query using a single connection
  std::unordered_map<int, FileMetadata> id_to_metadata;
  {
    PooledConnection conn(db_manager_);
    std::string ids_str = int_vector_to_comma_string(label_ids);
    *conn << "SELECT id, path, original_path, file_hash, processing_status, tags, "
                 "last_modified, created_at, file_type, file_size, summary_vector_blob, "
                 "suggested_category, suggested_filename FROM files WHERE id IN (" + ids_str + ")" >>
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
          metadata.content_hash = file_hash;
          if (processing_status)
            metadata.processing_status = processing_status_from_string(*processing_status);
          if (tags)
            metadata.tags = *tags;
          metadata.last_modified = string_to_time_point(last_modified);
          metadata.created_at = string_to_time_point(created_at);
          metadata.file_type = file_type_from_string(file_type);
          metadata.file_size = static_cast<size_t>(file_size);
          if (vector_blob && vector_blob->size() == VECTOR_DIMENSION * sizeof(float)) {
            const float *data = reinterpret_cast<const float *>(vector_blob->data());
            metadata.summary_vector_embedding.assign(data, data + VECTOR_DIMENSION);
          }
          if (suggested_category)
            metadata.suggested_category = *suggested_category;
          if (suggested_filename)
            metadata.suggested_filename = *suggested_filename;
          id_to_metadata[id] = std::move(metadata);
        };
  }

  // Assemble results in the same order as the labels/distances
  std::vector<FileSearchResult> results;
  results.reserve(label_ids.size());
  for (int i = 0; i < actual_k; ++i) {
    int id = static_cast<int>(labels[i]);
    if (id == -1)
      continue;
    auto it = id_to_metadata.find(id);
    if (it != id_to_metadata.end()) {
      results.push_back({id, distances[i], std::move(it->second)});
    } else {
      std::cerr << "Warning: Faiss returned ID " << id
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
    // Load vectors from DB into memory first to avoid holding a connection while enriching results
    std::vector<faiss::idx_t> faiss_ids;
    std::vector<float> all_vectors_flat;
    int current_num_vectors = 0;
    {
      PooledConnection conn(db_manager_);
      std::string file_ids_str = int_vector_to_comma_string(file_ids);
      *conn << "SELECT id, vector_blob FROM chunks WHERE file_id IN (" + file_ids_str +
                   ") ORDER BY file_id, chunk_index" >>
          [&](int64_t id, std::vector<char> vector_blob) {
            if (vector_blob.size() == VECTOR_DIMENSION * sizeof(float)) {
              faiss_ids.push_back(id);
              const float *vec_ptr = reinterpret_cast<const float *>(vector_blob.data());
              all_vectors_flat.insert(all_vectors_flat.end(), vec_ptr, vec_ptr + VECTOR_DIMENSION);
              current_num_vectors++;
            } else if (vector_blob.size() > 0) {
              std::cerr << "Warning: Skipping chunk ID " << id
                        << " during index rebuild due to mismatched vector dimension. Expected "
                        << VECTOR_DIMENSION * sizeof(float) << " bytes, got " << vector_blob.size()
                        << " bytes." << std::endl;
            }
          };
    }

    // Create temporary chunk index and perform FAISS search without holding DB connection
    auto chunk_index = std::unique_ptr<faiss::IndexIDMap>(create_base_index());
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

void MetadataStore::update_path_if_exists(const std::string& old_path, const std::string& new_path) {
  try {
    PooledConnection conn(db_manager_);
    *conn << "UPDATE files SET path = ?, original_path = ? WHERE path = ?"
          << new_path << new_path << old_path;
  } catch (const sqlite::sqlite_exception& e) {
    throw MetadataStoreError("Failed to update file path: " + std::string(e.what()));
  }
}

void MetadataStore::mark_removed_if_exists(const std::string& path) {
  try {
    PooledConnection conn(db_manager_);
    std::string removed_status = to_string(ProcessingStatus::FAILED); // Mark as failed for now
    *conn << "UPDATE files SET processing_status = ? WHERE path = ?"
          << removed_status << path;
  } catch (const sqlite::sqlite_exception& e) {
    throw MetadataStoreError("Failed to mark file as removed: " + std::string(e.what()));
  }
}

}