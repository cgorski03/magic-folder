#define SQLITE_HAS_CODEC 1
#define SQLCIPHER_CRYPTO_OPENSSL 1
#include <sqlcipher/sqlite3.h>
#include "magic_core/db/connection_pool.hpp"
#include <stdexcept>

namespace magic_core {

ConnectionPool::ConnectionPool(const std::string& db_path, const std::string& db_key, int pool_size)
    : db_path_(db_path), db_key_(db_key) {
  for (int i = 0; i < pool_size; ++i) {
    auto db = std::make_unique<sqlite::database>(db_path_);
    sqlite3* handle = db->connection().get();
    if (!handle) {
      throw std::runtime_error("Failed to get native handle for connection in pool.");
    }

    if (sqlite3_key(handle, db_key_.c_str(), db_key_.length()) != SQLITE_OK) {
      throw std::runtime_error("Failed to key database for connection in pool: " +
                               std::string(sqlite3_errmsg(handle)));
    }

    // Run a test query to ensure the key is correct
    *db << "SELECT count(*) FROM sqlite_master;";

    *db << "PRAGMA foreign_keys = ON;";
    *db << "PRAGMA journal_mode = WAL;";

    pool_.push(std::move(db));
  }
}

std::unique_ptr<sqlite::database> ConnectionPool::get_connection() {
  std::unique_lock<std::mutex> lock(mtx_);
  // Wait until a connection is available or shutdown is requested
  cv_.wait(lock, [this] { return shutting_down_ || !pool_.empty(); });

  if (shutting_down_) {
    throw std::runtime_error("Connection pool is shut down");
  }

  // Get the connection from the front of the queue
  std::unique_ptr<sqlite::database> conn = std::move(pool_.front());
  pool_.pop();
  return conn;
}

void ConnectionPool::return_connection(std::unique_ptr<sqlite::database> conn) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (!shutting_down_) {
    pool_.push(std::move(conn));
  }
  // Notify one waiting thread that a connection is available
  cv_.notify_one();
}

void ConnectionPool::shutdown() {
  std::lock_guard<std::mutex> lock(mtx_);
  shutting_down_ = true;
  while (!pool_.empty()) {
      pool_.pop();
  }
  cv_.notify_all();
}

}  // namespace magic_core