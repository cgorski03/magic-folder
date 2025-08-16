#pragma once
#include <sqlite_modern_cpp.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace magic_core {

class ConnectionPool {
public:
    ConnectionPool(const std::string& db_path, const std::string& db_key, int pool_size);

    std::unique_ptr<sqlite::database> get_connection();

    // Returns a connection to the pool.
    void return_connection(std::unique_ptr<sqlite::database> conn);
    void shutdown();

private:
    bool shutting_down_ = false;
    std::string db_path_;
    std::string db_key_;
    std::queue<std::unique_ptr<sqlite::database>> pool_;
    std::mutex mtx_;
    std::condition_variable cv_;
};

} // namespace magic_core