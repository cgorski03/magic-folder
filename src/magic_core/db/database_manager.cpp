#include "magic_core/db/database_manager.hpp"

#include <filesystem>

namespace magic_core {

DatabaseManager::DatabaseManager(const std::filesystem::path& db_path) {
  std::filesystem::create_directories(db_path.parent_path());
  open_database(db_path);
  run_pragmas();
  create_tables();
}

sqlite::database& DatabaseManager::get_db() { return *db_; }

void DatabaseManager::open_database(const std::filesystem::path& db_path) {
  db_ = std::make_unique<sqlite::database>(db_path.string());
  // Touch sqlite_master to ensure DB is open
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


