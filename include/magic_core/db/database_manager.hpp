#pragma once
#include <filesystem>
#include <memory>
#include <sqlite_modern_cpp.h>

namespace magic_core {

/**
 * DatabaseManager owns the SQLite connection (SQLCipher-enabled), applies the
 * encryption key, PRAGMAs, and runs all migrations (table/index creation).
 */
class DatabaseManager {
 public:
  explicit DatabaseManager(const std::filesystem::path& db_path);
  ~DatabaseManager() = default;

  // Non-copyable, movable
  DatabaseManager(const DatabaseManager&) = delete;
  DatabaseManager& operator=(const DatabaseManager&) = delete;
  DatabaseManager(DatabaseManager&&) noexcept = default;
  DatabaseManager& operator=(DatabaseManager&&) noexcept = default;

  sqlite::database& get_db();

 private:
  void open_database(const std::filesystem::path& db_path);
  void run_pragmas();
  void create_tables();

 private:
  std::unique_ptr<sqlite::database> db_;
};

}  // namespace magic_core


