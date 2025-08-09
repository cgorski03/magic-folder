#include <gtest/gtest.h>
#include <sqlite_modern_cpp.h>
#include <string>
#include <vector>

#include "../../common/utilities_test.hpp"

namespace magic_core {

class DatabaseManagerTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    magic_tests::MetadataStoreTestBase::SetUp();
  }
};

TEST_F(DatabaseManagerTest, CreatesSchema_OnConstruction) {
  // Verify required tables exist
  std::vector<std::string> required_tables = {"files", "chunks", "task_queue"};

  sqlite::database& db = db_manager_->get_db();
  for (const auto& table : required_tables) {
    int count = 0;
    db << "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?" << table >> count;
    EXPECT_EQ(count, 1) << "Missing table: " << table;
  }
}

TEST_F(DatabaseManagerTest, HasIndexesAndPragmas_Applied) {
  sqlite::database& db = db_manager_->get_db();

  // Check index on task_queue
  int idx_count = 0;
  db << "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name='idx_task_queue_status_priority'" >> idx_count;
  EXPECT_EQ(idx_count, 1);

  // Verify foreign_keys pragma is ON
  int fk_on = 0;
  db << "PRAGMA foreign_keys;" >> fk_on;
  EXPECT_EQ(fk_on, 1);
}

}  // namespace magic_core
