#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "magic_core/async/file_watcher_service.hpp"
#include "magic_core/services/file_processing_service.hpp"
#include "magic_core/types/chunk.hpp"
#include "utilities_test.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::StrictMock;
using ::testing::NiceMock;

namespace magic_tests {

// Mock backend for testing FileWatcherService logic without FSEvents
class MockFileWatcherBackend : public magic_core::async::IFileWatcherBackend {
public:
  MOCK_METHOD(void, start, (), (override));
  MOCK_METHOD(void, stop, (), (override));
  
  // Helper to simulate events
  void simulate_event(const magic_core::async::FileWatchEvent& event) {
    if (handler_) {
      handler_(event);
    }
  }
  
  void set_handler(Handler handler) {
    handler_ = std::move(handler);
  }
  
private:
  Handler handler_;
};

// Mock FileProcessingService for testing
class MockFileProcessingService : public magic_core::FileProcessingService {
public:
  // We need to provide a constructor since FileProcessingService doesn't have a default one
  MockFileProcessingService() : FileProcessingService(nullptr, nullptr, nullptr, nullptr) {}
  
  MOCK_METHOD(std::optional<long long>, request_processing, (const std::filesystem::path& file_path), (override));
};

// Test fixture that uses mocked backend
class FileWatcherServiceTest : public MetadataStoreTestBase {
protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    
    // Create temporary watch directory
    temp_watch_dir_ = std::filesystem::temp_directory_path() / "magic_folder_watch_test";
    std::error_code ec;
    std::filesystem::remove_all(temp_watch_dir_, ec);
    std::filesystem::create_directories(temp_watch_dir_);
    
    // Setup watch configuration
    watch_config_.drop_root = temp_watch_dir_;
    watch_config_.recursive = true;
    watch_config_.settle_ms = std::chrono::milliseconds(50);  // Very fast for testing
    watch_config_.modify_quiesce_ms = std::chrono::milliseconds(100);  // Very fast for testing
    watch_config_.sweep_interval = std::chrono::milliseconds(50);  // Very fast for testing
    watch_config_.reindex_batch_size = 10;
    
    // Create mock backend
    mock_backend_ = std::make_unique<StrictMock<MockFileWatcherBackend>>();
    mock_backend_ptr_ = mock_backend_.get();
    
    // Create mock file processing service
    mock_file_processing_service_ = std::make_unique<NiceMock<MockFileProcessingService>>();
    
    // Configure mock to return task IDs and create actual tasks in TaskQueueRepo
    ON_CALL(*mock_file_processing_service_, request_processing(_))
        .WillByDefault([this](const std::filesystem::path& path) -> std::optional<long long> {
          // Simulate what FileProcessingService would do: create a task in the repo
          try {
            return task_queue_repo_->create_file_process_task("PROCESS_FILE", path.string(), 10);
          } catch (...) {
            return std::nullopt;
          }
        });
    
    // Create testable file watcher service
    file_watcher_ = std::make_unique<TestableFileWatcherService>(
        watch_config_, *mock_file_processing_service_, *task_queue_repo_, *metadata_store_, std::move(mock_backend_));
  }

  void TearDown() override {
    if (file_watcher_ && file_watcher_->is_running()) {
      EXPECT_CALL(*mock_backend_ptr_, stop()).Times(1);
      file_watcher_->stop();
    }
    file_watcher_.reset();
    
    std::error_code ec;
    std::filesystem::remove_all(temp_watch_dir_, ec);
    
    MetadataStoreTestBase::TearDown();
  }

  // Helper to create test files
  void create_test_file(const std::string& filename, const std::string& content = "test content") {
    auto file_path = temp_watch_dir_ / filename;
    std::ofstream file(file_path);
    file << content;
    file.close();
  }

  // Helper to simulate file events
  void simulate_file_created(const std::string& filename) {
    magic_core::async::FileWatchEvent event;
    event.path = temp_watch_dir_ / filename;
    event.kind = magic_core::async::EventKind::Created;
    event.is_dir = false;
    event.ts = std::chrono::system_clock::now();
    
    mock_backend_ptr_->simulate_event(event);
  }

  void simulate_file_modified(const std::string& filename) {
    magic_core::async::FileWatchEvent event;
    event.path = temp_watch_dir_ / filename;
    event.kind = magic_core::async::EventKind::Modified;
    event.is_dir = false;
    event.ts = std::chrono::system_clock::now();
    
    mock_backend_ptr_->simulate_event(event);
  }

  void simulate_file_deleted(const std::string& filename) {
    magic_core::async::FileWatchEvent event;
    event.path = temp_watch_dir_ / filename;
    event.kind = magic_core::async::EventKind::Deleted;
    event.is_dir = false;
    event.ts = std::chrono::system_clock::now();
    
    mock_backend_ptr_->simulate_event(event);
  }

  void simulate_overflow() {
    magic_core::async::FileWatchEvent event;
    event.kind = magic_core::async::EventKind::Overflow;
    event.ts = std::chrono::system_clock::now();
    
    mock_backend_ptr_->simulate_event(event);
  }

  // Wait for settle/sweep operations to complete
  void wait_for_processing(int timeout_ms = 500) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
  }

  // Testable version that accepts a mock backend
  class TestableFileWatcherService : public magic_core::async::FileWatcherService {
  public:
    TestableFileWatcherService(const magic_core::async::WatchConfig& cfg,
                              magic_core::FileProcessingService& file_processing_service,
                              magic_core::TaskQueueRepo& task_queue_repo,
                              magic_core::MetadataStore& metadata,
                              std::unique_ptr<magic_core::async::IFileWatcherBackend> backend)
        : FileWatcherService(cfg, file_processing_service, task_queue_repo, metadata) {
      // Replace the backend created in the constructor
      backend_ = std::move(backend);
      
      // Set up the handler in the mock
      auto* mock = static_cast<MockFileWatcherBackend*>(backend_.get());
      mock->set_handler([this](const magic_core::async::FileWatchEvent& ev) {
        this->on_backend_event(ev);
      });
    }
    
    // Expose protected method for testing
    void test_on_backend_event(const magic_core::async::FileWatchEvent& ev) {
      on_backend_event(ev);
    }
    
    // Access to backend for testing
    magic_core::async::IFileWatcherBackend* get_backend() { return backend_.get(); }
  };

  std::filesystem::path temp_watch_dir_;
  magic_core::async::WatchConfig watch_config_;
  std::unique_ptr<TestableFileWatcherService> file_watcher_;
  std::unique_ptr<StrictMock<MockFileWatcherBackend>> mock_backend_;
  StrictMock<MockFileWatcherBackend>* mock_backend_ptr_;
  std::unique_ptr<NiceMock<MockFileProcessingService>> mock_file_processing_service_;
};

// Basic functionality tests
TEST_F(FileWatcherServiceTest, ConstructorCreatesValidInstance) {
  EXPECT_FALSE(file_watcher_->is_running());
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, 0);
  EXPECT_EQ(stats.files_enqueued, 0);
  EXPECT_EQ(stats.files_marked_dirty, 0);
}

TEST_F(FileWatcherServiceTest, StartAndStopService) {
  EXPECT_FALSE(file_watcher_->is_running());
  
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  EXPECT_TRUE(file_watcher_->is_running());
  
  EXPECT_CALL(*mock_backend_ptr_, stop()).Times(1);
  file_watcher_->stop();
  EXPECT_FALSE(file_watcher_->is_running());
  
  // Should be safe to call stop multiple times (no additional mock calls expected)
  file_watcher_->stop();
  EXPECT_FALSE(file_watcher_->is_running());
}

TEST_F(FileWatcherServiceTest, InitialScanDetectsExistingFiles) {
  // Create some files before scanning
  create_test_file("existing1.txt", "content1");
  create_test_file("existing2.txt", "content2");
  
  // Run initial scan
  file_watcher_->initial_scan();
  
  auto stats = file_watcher_->stats();
  EXPECT_GT(stats.events_seen, 0);  // Should detect existing files
  EXPECT_EQ(stats.scans_performed, 1);
}

// Event handling tests
TEST_F(FileWatcherServiceTest, HandlesFileCreatedEvents) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  create_test_file("test.txt", "content");
  simulate_file_created("test.txt");
  
  // Wait for settle processing
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, 1);
  EXPECT_GT(stats.files_enqueued, 0);
  
  // Check that a task was enqueued
  auto pending_tasks = task_queue_repo_->get_tasks_by_status(magic_core::TaskStatus::PENDING);
  EXPECT_GT(pending_tasks.size(), 0);
  
  if (!pending_tasks.empty()) {
    EXPECT_EQ(pending_tasks[0].task_type, "PROCESS_FILE");
    EXPECT_EQ(pending_tasks[0].priority, 10);
  }
}

TEST_F(FileWatcherServiceTest, HandlesFileModifiedEvents) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  create_test_file("test.txt", "content");
  simulate_file_modified("test.txt");
  
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, 1);
  EXPECT_GT(stats.files_marked_dirty, 0);
}

TEST_F(FileWatcherServiceTest, HandlesFileDeletedEvents) {
  // Add file to metadata store first
  create_test_file("test.txt", "content");
  auto basic_metadata = TestUtilities::create_test_basic_file_metadata(
      (temp_watch_dir_ / "test.txt").string());
  metadata_store_->upsert_file_stub(basic_metadata);
  
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  simulate_file_deleted("test.txt");
  
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, 1);
}

TEST_F(FileWatcherServiceTest, HandlesOverflowEvents) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  // Create some files for the overflow to find
  create_test_file("overflow1.txt", "content1");
  create_test_file("overflow2.txt", "content2");
  const int num_files = 2;
  
  // Get initial stats
  auto initial_stats = file_watcher_->stats();
  
  simulate_overflow();
  
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  // Should see: 1 overflow event + events from the files found during rescan
  EXPECT_GE(stats.events_seen, initial_stats.events_seen + 1);  // At least the overflow event
  EXPECT_EQ(stats.overflows, initial_stats.overflows + 1);
  EXPECT_GT(stats.scans_performed, initial_stats.scans_performed);  // Should trigger rescan
}

// File filtering tests
TEST_F(FileWatcherServiceTest, IgnoresDirectoryEvents) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  magic_core::async::FileWatchEvent dir_event;
  dir_event.path = temp_watch_dir_ / "some_dir";
  dir_event.kind = magic_core::async::EventKind::Created;
  dir_event.is_dir = true;  // This should be ignored
  
  mock_backend_ptr_->simulate_event(dir_event);
  
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, 1);
  EXPECT_EQ(stats.files_enqueued, 0);  // Should not enqueue directories
}

TEST_F(FileWatcherServiceTest, IgnoresConfiguredPatterns) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  // Test various ignore patterns
  std::vector<std::string> ignored_files = {
    ".DS_Store", "Thumbs.db", ".Spotlight-V100", ".fseventsd",
    "temp.tmp", "download.part", "file.crdownload"
  };
  
  for (const auto& filename : ignored_files) {
    create_test_file(filename, "ignored content");
    simulate_file_created(filename);
  }
  
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, ignored_files.size());
  EXPECT_EQ(stats.files_enqueued, 0);  // All should be ignored
  
  auto pending_tasks = task_queue_repo_->get_tasks_by_status(magic_core::TaskStatus::PENDING);
  EXPECT_EQ(pending_tasks.size(), 0);
}

TEST_F(FileWatcherServiceTest, ProcessesNonIgnoredFiles) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  std::vector<std::string> valid_files = {
    "document.txt", "README.md", "script.py", "data.json"
  };
  
  for (const auto& filename : valid_files) {
    create_test_file(filename, "valid content");
    simulate_file_created(filename);
  }
  
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, valid_files.size());
  EXPECT_GT(stats.files_enqueued, 0);
  
  auto pending_tasks = task_queue_repo_->get_tasks_by_status(magic_core::TaskStatus::PENDING);
  EXPECT_GT(pending_tasks.size(), 0);
}

// Path filtering tests
TEST_F(FileWatcherServiceTest, IgnoresFilesOutsideWatchRoot) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  // Create event for file outside watch root
  magic_core::async::FileWatchEvent outside_event;
  outside_event.path = "/tmp/outside_file.txt";
  outside_event.kind = magic_core::async::EventKind::Created;
  outside_event.is_dir = false;
  
  mock_backend_ptr_->simulate_event(outside_event);
  
  wait_for_processing();
  
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, 1);
  EXPECT_EQ(stats.files_enqueued, 0);  // Should be ignored
}

// Settle logic tests
TEST_F(FileWatcherServiceTest, SettleLogicWaitsForStableFiles) {
  // Use longer settle time for this test
  watch_config_.settle_ms = std::chrono::milliseconds(200);
  auto test_watcher = std::make_unique<TestableFileWatcherService>(
      watch_config_, *mock_file_processing_service_, *task_queue_repo_, *metadata_store_, 
      std::make_unique<NiceMock<MockFileWatcherBackend>>());
  
  auto* test_backend = static_cast<NiceMock<MockFileWatcherBackend>*>(test_watcher->get_backend());
  test_backend->set_handler([&test_watcher](const magic_core::async::FileWatchEvent& ev) {
    test_watcher->test_on_backend_event(ev);
  });
  
  test_watcher->start();
  
  create_test_file("settle_test.txt", "initial");
  
  // Simulate file creation
  magic_core::async::FileWatchEvent event;
  event.path = temp_watch_dir_ / "settle_test.txt";
  event.kind = magic_core::async::EventKind::Created;
  event.is_dir = false;
  test_backend->simulate_event(event);
  
  // Check immediately - should have no pending tasks yet
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto pending_tasks = task_queue_repo_->get_tasks_by_status(magic_core::TaskStatus::PENDING);
  EXPECT_EQ(pending_tasks.size(), 0);
  
  // Wait for settle time
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  
  // Now should have pending tasks
  pending_tasks = task_queue_repo_->get_tasks_by_status(magic_core::TaskStatus::PENDING);
  EXPECT_GT(pending_tasks.size(), 0);
  
  test_watcher->stop();
}

// Dirty file reindexing tests
TEST_F(FileWatcherServiceTest, ReindexesDirtyFilesAfterQuiescence) {
  // Use short quiesce time for testing
  watch_config_.modify_quiesce_ms = std::chrono::milliseconds(100);
  watch_config_.sweep_interval = std::chrono::milliseconds(50);
  
  auto test_watcher = std::make_unique<TestableFileWatcherService>(
      watch_config_, *mock_file_processing_service_, *task_queue_repo_, *metadata_store_, 
      std::make_unique<NiceMock<MockFileWatcherBackend>>());
  
  auto* test_backend = static_cast<NiceMock<MockFileWatcherBackend>*>(test_watcher->get_backend());
  test_backend->set_handler([&test_watcher](const magic_core::async::FileWatchEvent& ev) {
    test_watcher->test_on_backend_event(ev);
  });
  
  test_watcher->start();
  
  create_test_file("dirty_test.txt", "initial");
  
  // Simulate file modification
  magic_core::async::FileWatchEvent event;
  event.path = temp_watch_dir_ / "dirty_test.txt";
  event.kind = magic_core::async::EventKind::Modified;
  event.is_dir = false;
  test_backend->simulate_event(event);
  
  // Wait for quiesce time and sweeper to run
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  
  // Should have reindex task
  auto pending_tasks = task_queue_repo_->get_tasks_by_status(magic_core::TaskStatus::PENDING);
  bool found_reindex = false;
  for (const auto& task : pending_tasks) {
    if (task.task_type == "REINDEX_FILE") {
      found_reindex = true;
      EXPECT_EQ(task.priority, 8);
      break;
    }
  }
  EXPECT_TRUE(found_reindex);
  
  test_watcher->stop();
}

// Statistics tests
TEST_F(FileWatcherServiceTest, StatisticsAccuratelyTrackEvents) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  auto initial_stats = file_watcher_->stats();
  
  // Simulate various events
  create_test_file("stats1.txt", "content1");
  simulate_file_created("stats1.txt");
  
  create_test_file("stats2.txt", "content2");
  simulate_file_created("stats2.txt");
  
  simulate_file_modified("stats1.txt");
  
  wait_for_processing();
  
  auto final_stats = file_watcher_->stats();
  
  EXPECT_EQ(final_stats.events_seen, initial_stats.events_seen + 3);
  EXPECT_GT(final_stats.files_enqueued, initial_stats.files_enqueued);
  EXPECT_GT(final_stats.files_marked_dirty, initial_stats.files_marked_dirty);
}

// Error handling tests
TEST_F(FileWatcherServiceTest, HandlesNonExistentWatchDirectory) {
  auto nonexistent_dir = std::filesystem::temp_directory_path() / "nonexistent_magic_folder_test";
  magic_core::async::WatchConfig bad_config;
  bad_config.drop_root = nonexistent_dir;
  
  auto bad_watcher = std::make_unique<TestableFileWatcherService>(
      bad_config, *mock_file_processing_service_, *task_queue_repo_, *metadata_store_,
      std::make_unique<NiceMock<MockFileWatcherBackend>>());
  
  // Should not crash
  bad_watcher->initial_scan();
  
  auto stats = bad_watcher->stats();
  EXPECT_EQ(stats.scans_performed, 1);
}

// Integration with dependencies tests
TEST_F(FileWatcherServiceTest, IntegratesWithTaskQueueRepo) {
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  // Create multiple files
  std::vector<std::string> files = {"integration1.txt", "integration2.txt", "integration3.txt"};
  
  for (const auto& filename : files) {
    create_test_file(filename, "content");
    simulate_file_created(filename);
  }
  
  wait_for_processing();
  
  auto pending_tasks = task_queue_repo_->get_tasks_by_status(magic_core::TaskStatus::PENDING);
  EXPECT_GE(pending_tasks.size(), files.size());
  
  // All should be PROCESS_FILE tasks with correct priority
  for (const auto& task : pending_tasks) {
    EXPECT_EQ(task.task_type, "PROCESS_FILE");
    EXPECT_EQ(task.priority, 10);
    ASSERT_TRUE(task.target_path.has_value());
    EXPECT_FALSE(task.target_path->empty());
  }
}

TEST_F(FileWatcherServiceTest, IntegratesWithMetadataStore) {
  // Add a file to metadata store first
  create_test_file("metadata_test.txt", "content");
  auto basic_metadata = TestUtilities::create_test_basic_file_metadata(
      (temp_watch_dir_ / "metadata_test.txt").string());
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);
  EXPECT_GT(file_id, 0);
  
  EXPECT_CALL(*mock_backend_ptr_, start()).Times(1);
  file_watcher_->start();
  
  // Simulate file deletion
  simulate_file_deleted("metadata_test.txt");
  
  wait_for_processing();
  
  // Should not crash and should handle the metadata update
  auto stats = file_watcher_->stats();
  EXPECT_EQ(stats.events_seen, 1);
}

}  // namespace magic_tests