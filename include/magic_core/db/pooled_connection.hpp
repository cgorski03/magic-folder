#pragma once
#include "magic_core/db/database_manager.hpp"
#include <sqlite_modern_cpp.h>
#include <memory>

namespace magic_core {
class PooledConnection {
public:
    // Constructor gets a connection from the manager's pool
    explicit PooledConnection(DatabaseManager& manager)
    : manager_(manager), conn_(manager.get_connection()) {
    if (!conn_) {
        throw std::runtime_error("Failed to acquire database connection: system is shutting down.");
    }
}
    // Destructor automatically returns the connection
    ~PooledConnection() {
        if (conn_) {
            manager_.return_connection(std::move(conn_));
        }
    }

    // Allow access to the underlying database object
    sqlite::database* operator->() const { return conn_.get(); }
    sqlite::database& operator*() const { return *conn_; }

    // Delete copy/move to prevent ownership issues
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;

private:
    magic_core::DatabaseManager& manager_;
    std::unique_ptr<sqlite::database> conn_;
};
}  // namespace magic_core