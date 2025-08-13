#define SQLITE_HAS_CODEC 1
#define SQLCIPHER_CRYPTO_OPENSSL 1
#include "magic_core/db/database_manager.hpp"

namespace magic_core {

DatabaseManager::DatabaseManager(const std::filesystem::path& db_path, const std::string& db_key) {
  std::filesystem::create_directories(db_path.parent_path());
  open_database(db_path, db_key);
  run_pragmas();
  create_tables();
}

sqlite::database& DatabaseManager::get_db() {
  return *db_;
}

void DatabaseManager::open_database(const std::filesystem::path& db_path,
                                    const std::string& db_key) {
  db_ = std::make_unique<sqlite::database>(db_path.string());
  sqlite3* handle = db_->connection().get();
  if (!handle) {
    throw std::runtime_error("Failed to get native database handle after opening.");
  }

  if (sqlite3_key(handle, db_key.c_str(), db_key.length()) != SQLITE_OK) {
    std::string error_msg = sqlite3_errmsg(handle);
    throw std::runtime_error("Failed to key database: " + error_msg);
  }

  *db_ << "SELECT count(*) FROM sqlite_master;";
}

void DatabaseManager::run_pragmas() {
  *db_ << "PRAGMA foreign_keys = ON;";
}

void DatabaseManager::create_tables() {
  // files
  *db_ << R"(
    CREATE TABLE IF NOT EXISTS files (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        path TEXT UNIQUE NOT NULL,
        original_path TEXT,
        file_hash TEXT NOT NULL,
        processing_status TEXT NOT NULL,
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

  // chunks
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

  // task_queue
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

  *db_ << R"(
    CREATE INDEX IF NOT EXISTS idx_task_queue_status_priority 
    ON task_queue(status, priority, created_at)
  )";
}

}  // namespace magic_core
