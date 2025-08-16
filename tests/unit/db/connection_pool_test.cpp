#include <gtest/gtest.h>
#include <future>
#include <atomic>
#include <string>

#include "../../common/utilities_test.hpp"
#include "magic_core/db/database_manager.hpp"
#include "magic_core/db/pooled_connection.hpp"

namespace magic_core {

class ConnectionPoolTest : public ::testing::Test {
protected:
  void SetUp() override {
    temp_db_path_ = magic_tests::TestUtilities::create_temp_test_db();
    const std::string test_db_key = "magic_folder_test_key";
    // Ensure schema exists before spinning connections
    auto &mgr = DatabaseManager::get_instance();
    mgr.shutdown();
    // Use a larger pool in this suite to validate multi-connection behavior
    mgr.initialize(temp_db_path_, test_db_key, /*pool_size*/ 4);
  }

  void TearDown() override {
    magic_tests::TestUtilities::cleanup_temp_db(temp_db_path_);
  }

  std::filesystem::path temp_db_path_;
};

TEST_F(ConnectionPoolTest, CanBorrowAndReturnConnections) {
  auto &mgr = DatabaseManager::get_instance();

  // Borrow two connections
  PooledConnection c1(mgr);
  PooledConnection c2(mgr);

  int count = 0;
  *c1 << "SELECT COUNT(*) FROM sqlite_master" >> count;
  EXPECT_GE(count, 0);
}

TEST_F(ConnectionPoolTest, BlocksWhenPoolExhaustedAndResumes) {
  auto &mgr = DatabaseManager::get_instance();

  // Exhaust pool (size=4 from SetUp)
  auto holder1 = std::make_unique<PooledConnection>(mgr);
  auto holder2 = std::make_unique<PooledConnection>(mgr);
  auto holder3 = std::make_unique<PooledConnection>(mgr);
  auto holder4 = std::make_unique<PooledConnection>(mgr);

  std::promise<void> start_promise;
  std::shared_future<void> start_future(start_promise.get_future());

  // Request another connection on another thread, which should block until one is returned
  std::atomic<bool> acquired{false};
  std::thread t([&]() {
    start_future.wait();
    PooledConnection c3(mgr);
    int count = 0;
    *c3 << "SELECT COUNT(*) FROM sqlite_master" >> count;
    acquired.store(true);
  });

  // Let thread start and block, then release one connection
  start_promise.set_value();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  holder1.reset(); // returns connection to pool

  t.join();
  EXPECT_TRUE(acquired.load());
}

} // namespace magic_core


