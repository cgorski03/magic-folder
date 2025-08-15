#define SQLITE_HAS_CODEC 1
#define SQLCIPHER_CRYPTO_OPENSSL 1
#include <sqlcipher/sqlite3.h>

#include "magic_core/db/database_manager.hpp"

#include <stdexcept>

namespace magic_core {

DatabaseManager& DatabaseManager::get_instance() {
  static DatabaseManager instance;
  return instance;
}

void DatabaseManager::initialize(const std::filesystem::path& db_path,
                                 const std::string& db_key,
                                 int pool_size) {
  if (is_initialized_) {
    return;
  }

  // Ensure parent directory exists
  std::filesystem::create_directories(db_path.parent_path());

  // 1. Perform one-time schema setup before creating the pool
  setup_schema(db_path, db_key);

  // 2. Create the connection pool for workers to use
  pool_ = std::make_unique<ConnectionPool>(db_path.string(), db_key, pool_size);

  is_initialized_ = true;
}
void DatabaseManager::shutdown() {
  if (!is_initialized_) {
    return;
  }
  pool_->shutdown();
  is_initialized_ = false;
}
std::unique_ptr<sqlite::database> DatabaseManager::get_connection() {
  if (!is_initialized_) {
    throw std::runtime_error("DatabaseManager has not been initialized.");
  }
  return pool_->get_connection();
}

void DatabaseManager::return_connection(std::unique_ptr<sqlite::database> conn) {
  if (!is_initialized_) {
    return;
  }
  pool_->return_connection(std::move(conn));
}

void DatabaseManager::setup_schema(const std::filesystem::path& db_path,
                                   const std::string& db_key) {
  // Use a temporary, single-use connection just for schema setup.
  // This ensures table creation is a single, non-pooled operation.
  sqlite::database db(db_path.string());
  sqlite3* handle = db.connection().get();
  if (!handle) {
    throw std::runtime_error("Setup: Failed to get native database handle.");
  }
  if (sqlite3_key(handle, db_key.c_str(), db_key.length()) != SQLITE_OK) {
    throw std::runtime_error("Setup: Failed to key database: " +
                             std::string(sqlite3_errmsg(handle)));
  }
  db << "SELECT count(*) FROM sqlite_master;";
  db << "PRAGMA foreign_keys = ON;";
  db << "PRAGMA journal_mode = WAL;";

  db << R"(
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
  db << R"(
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
  db << R"(
      CREATE TABLE IF NOT EXISTS task_queue (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          task_‰type TEXT NOT NULL,
          status TEXT DEFAULT 'PENDING',
          priority INTEGER DEFAULT 10,
          error_message TEXT,
          created_at TEXT NOT NULL,
          updated_at TEXT NOT NULL
          target_path TEXT NULL, -- Used by PROCESS_FILE, DELETE_FILE, etc.
          target_tag TEXT NULL,  -- Used by RETROACTIVE_TAG, RENAME_TAG, etc.
          payload TEXT NULL      -- For less common or complex arguments like 'keywords'
      )
    )";
  db << R"(
        CREATE TABLE task_progress (
        task_id INTEGER PRIMARY KEY, -- This is a FOREIGN KEY to task_queue.id
        progress_percent REAL NOT NULL DEFAULT 0.0,
        status_message TEXT NOT NULL DEFAULT 'Initializing...',
        updated_at INTEGER NOT NULL,
        FOREIGN KEY (task_id) REFERENCES TaskQueue(id) ON DELETE CASCADE
    );
    )";
  db << R"(
      CREATE INDEX IF NOT EXISTS idx_task_queue_status_priority 
      ON task_queue(status, priority, created_at)
    )";
}

}  // namespace magic_core