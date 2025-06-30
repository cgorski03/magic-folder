#include "magic_core/metadata_store.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

namespace magic_core
{

  MetadataStore::MetadataStore(const std::filesystem::path &db_path)
      : db_path_(db_path), db_(nullptr)
  {
    initialize();
  }

  MetadataStore::~MetadataStore()
  {
    if (db_)
    {
      sqlite3_close(db_);
    }
  }

  MetadataStore::MetadataStore(MetadataStore &&other) noexcept
      : db_path_(std::move(other.db_path_)), db_(other.db_)
  {
    other.db_ = nullptr;
  }

  MetadataStore &MetadataStore::operator=(MetadataStore &&other) noexcept
  {
    if (this != &other)
    {
      if (db_)
      {
        sqlite3_close(db_);
      }
      db_path_ = std::move(other.db_path_);
      db_ = other.db_;
      other.db_ = nullptr;
    }
    return *this;
  }

  void MetadataStore::initialize()
  {
    // Create database directory if it doesn't exist
    std::filesystem::create_directories(db_path_.parent_path());

    // Open database
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK)
    {
      throw MetadataStoreError("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
    }

    create_tables();
  }

  void MetadataStore::create_tables()
  {
    const char *sql = R"(
        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT UNIQUE NOT NULL,
            content_hash TEXT NOT NULL,
            last_modified TEXT NOT NULL,
            created_at TEXT NOT NULL,
            file_type TEXT NOT NULL,
            file_size INTEGER NOT NULL
        );
    )";

    execute_sql(sql);
  }

  void MetadataStore::execute_sql(const std::string &sql)
  {
    char *err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK)
    {
      std::string error = "SQL execution failed: " + std::string(err_msg);
      sqlite3_free(err_msg);
      throw MetadataStoreError(error);
    }
  }

  std::string MetadataStore::compute_content_hash(const std::filesystem::path &file_path)
  {
    std::ifstream file(file_path, std::ios::binary);
    if (!file)
    {
      throw MetadataStoreError("Failed to open file for hashing: " + file_path.string());
    }

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char buffer[1024];
    while (file.read(buffer, sizeof(buffer)))
    {
      SHA256_Update(&sha256, buffer, file.gcount());
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
      ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
  }

  std::chrono::system_clock::time_point MetadataStore::get_file_last_modified(const std::filesystem::path &file_path)
  {
    auto ftime = std::filesystem::last_write_time(file_path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return sctp;
  }

  void MetadataStore::upsert_file_metadata(const FileMetadata &metadata)
  {
    // Convert time points to strings
    auto time_to_string = [](const std::chrono::system_clock::time_point &tp)
    {
      auto time_t = std::chrono::system_clock::to_time_t(tp);
      std::stringstream ss;
      ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
      return ss.str();
    };

    std::string last_modified_str = time_to_string(metadata.last_modified);
    std::string created_at_str = time_to_string(metadata.created_at);

    // Use REPLACE to handle upsert
    std::string sql = "REPLACE INTO files (path, content_hash, last_modified, created_at, file_type, file_size) "
                      "VALUES (?, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, metadata.path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, metadata.content_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, last_modified_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, created_at_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, metadata.file_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, metadata.file_size);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
      sqlite3_finalize(stmt);
      throw MetadataStoreError("Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_finalize(stmt);
  }

  std::optional<FileMetadata> MetadataStore::get_file_metadata(const std::string &path)
  {
    std::string sql = "SELECT id, path, content_hash, last_modified, created_at, file_type, file_size "
                      "FROM files WHERE path = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      FileMetadata metadata;
      metadata.id = sqlite3_column_int(stmt, 0);
      metadata.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      metadata.content_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      // Parse time strings back to time points (simplified)
      metadata.file_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
      metadata.file_size = sqlite3_column_int64(stmt, 6);

      sqlite3_finalize(stmt);
      return metadata;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
  }

  bool MetadataStore::file_exists(const std::string &path)
  {
    return get_file_metadata(path).has_value();
  }

  std::vector<FileMetadata> MetadataStore::list_all_files()
  {
    std::vector<FileMetadata> files;
    std::string sql = "SELECT id, path, content_hash, last_modified, created_at, file_type, file_size FROM files";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
      FileMetadata metadata;
      metadata.id = sqlite3_column_int(stmt, 0);
      metadata.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      metadata.content_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      metadata.file_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
      metadata.file_size = sqlite3_column_int64(stmt, 6);

      files.push_back(metadata);
    }

    sqlite3_finalize(stmt);
    return files;
  }

  void MetadataStore::delete_file_metadata(const std::string &path)
  {
    std::string sql = "DELETE FROM files WHERE path = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      throw MetadataStoreError("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
      sqlite3_finalize(stmt);
      throw MetadataStoreError("Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_finalize(stmt);
  }

} // namespace magic_core