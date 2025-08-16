#include <gtest/gtest.h>
#include <sqlite_modern_cpp.h>
#include <string>
#include <vector>

#include "../../common/utilities_test.hpp"
#include "magic_core/db/pooled_connection.hpp"
#include "magic_core/db/connection_pool.hpp"

namespace magic_core {

class DatabaseManagerTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    magic_tests::MetadataStoreTestBase::SetUp();
  }
};

TEST_F(DatabaseManagerTest, CreatesSchema_OnInitialization) {
  // Verify required tables exist
  std::vector<std::string> required_tables = {"files", "chunks", "task_queue"};

  PooledConnection conn(*db_manager_);
  for (const auto& table : required_tables) {
    int count = 0;
    *conn << "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?" << table >> count;
    EXPECT_EQ(count, 1) << "Missing table: " << table;
  }
}

TEST_F(DatabaseManagerTest, HasIndexesAndPragmas_Applied) {
  PooledConnection conn(*db_manager_);

  // Check index on task_queue
  int idx_count = 0;
  *conn << "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name='idx_task_queue_status_priority'" >> idx_count;
  EXPECT_EQ(idx_count, 1);

  // Verify foreign_keys pragma is ON
  int fk_on = 0;
  *conn << "PRAGMA foreign_keys;" >> fk_on;
  EXPECT_EQ(fk_on, 1);
}

TEST_F(DatabaseManagerTest, ReopenWithWrongKey_Fails) {
  // Simulate a new initialization attempt with the wrong key: create a fresh pool with wrong key
  const std::string wrong_key = "incorrect_test_key";
  EXPECT_THROW({
                  magic_core::ConnectionPool bad_pool(temp_db_path_.string(), wrong_key, 1);
                },
                std::exception);
}

}  // namespace magic_core
