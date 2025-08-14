#pragma once

#include "magic_core/db/connection_pool.hpp"
#include <filesystem>
#include <memory>
#include <string>

namespace magic_core {

class DatabaseManager {
public:
    // Singleton access
    static DatabaseManager& get_instance();

    // Must be called once at application startup
    void initialize(const std::filesystem::path& db_path, const std::string& db_key, int pool_size);

    // These methods are used by the PooledConnection guard
    std::unique_ptr<sqlite::database> get_connection();
    void return_connection(std::unique_ptr<sqlite::database> conn);

    void shutdown();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

private:
    DatabaseManager() = default;
    void setup_schema(const std::filesystem::path& db_path, const std::string& db_key);

    std::unique_ptr<ConnectionPool> pool_;
    bool is_initialized_ = false;
};

} // namespace magic_core