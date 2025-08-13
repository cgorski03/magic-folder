#pragma once
#define SQLITE_HAS_CODEC 1
#define SQLCIPHER_CRYPTO_OPENSSL 1
#include <filesystem>
#include <memory>

#include <sqlcipher/sqlite3.h>
#include <sqlite_modern_cpp.h>

namespace magic_core {
/**
 * DatabaseManager owns the SQLite connection (SQLCipher-enabled), applies the
 * encryption key, PRAGMAs, and runs all migrations (table/index creation).
 */
class DatabaseManager {
 public:
  explicit DatabaseManager(const std::filesystem::path& db_path, const std::string& db_key);
  ~DatabaseManager() = default;

  // Non-copyable, movable
  DatabaseManager(const DatabaseManager&) = delete;
  DatabaseManager& operator=(const DatabaseManager&) = delete;
  DatabaseManager(DatabaseManager&&) noexcept = default;
  DatabaseManager& operator=(DatabaseManager&&) noexcept = default;

  sqlite::database& get_db();

 private:
  void open_database(const std::filesystem::path& db_path, const std::string& db_key);
  void run_pragmas();
  void create_tables();

 private:
  std::unique_ptr<sqlite::database> db_;
};

}  // namespace magic_core


