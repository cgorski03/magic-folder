#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <memory>
#include <fstream>
#include <set>

#include "magic_core/async/service_provider.hpp"
#include "magic_core/async/worker.hpp"
#include "magic_core/db/metadata_store.hpp"
#include "mocks_test.hpp"
#include "utilities_test.hpp"

namespace magic_tests {

using namespace magic_core;
using namespace magic_core::async;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

class WorkerTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    magic_tests::MetadataStoreTestBase::SetUp();
    
    // Create mock dependencies
    mock_ollama_client_ = std::make_shared<StrictMock<MockOllamaClient>>();
    mock_content_extractor_factory_ = std::make_shared<StrictMock<MockContentExtractorFactory>>();
    mock_content_extractor_ = std::make_unique<StrictMock<MockContentExtractor>>();
    
    // Create service provider with all dependencies
    service_provider_ = std::make_shared<ServiceProvider>(
        metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_
    );
    
    // Create worker with service provider
    worker_ = std::make_unique<Worker>(1, service_provider_);
  }

  void TearDown() override {
    worker_.reset();
    service_provider_.reset();
    mock_content_extractor_.reset();
    mock_content_extractor_factory_.reset();
    mock_ollama_client_.reset();
    magic_tests::MetadataStoreTestBase::TearDown();
  }

  // Helper method to create a test file on disk
  std::filesystem::path create_test_file(const std::string& content = "Test file content") {
    auto temp_file = std::filesystem::temp_directory_path() / ("test_file_" + std::to_string(std::rand()) + ".txt");
    std::ofstream file(temp_file);
    file << content;
    file.close();
    return temp_file;
  }

  // Helper method to cleanup test file
  void cleanup_test_file(const std::filesystem::path& file_path) {
    if (std::filesystem::exists(file_path)) {
      std::filesystem::remove(file_path);
    }
  }

  std::unique_ptr<Worker> worker_;
  std::shared_ptr<ServiceProvider> service_provider_;
  std::shared_ptr<StrictMock<MockOllamaClient>> mock_ollama_client_;
  std::shared_ptr<StrictMock<MockContentExtractorFactory>> mock_content_extractor_factory_;
  std::shared_ptr<StrictMock<MockContentExtractor>> mock_content_extractor_;
};

TEST_F(WorkerTest, RunOneTaskWithNoPendingTasks) {
  // When there are no pending tasks, run_one_task should return false
  bool result = worker_->run_one_task();
  EXPECT_FALSE(result);
}

TEST_F(WorkerTest, RunOneTaskWithPendingTask) {
  // Create a test file on disk
  auto test_file_path = create_test_file("This is test content for processing.");
  // Insert file stub required by worker
  {
    BasicFileMetadata stub = magic_tests::TestUtilities::create_test_basic_file_metadata(
        test_file_path.string(),
        "test_content_hash_123",
        FileType::Text,
        static_cast<size_t>(std::filesystem::file_size(test_file_path)),
        ProcessingStatus::QUEUED);
    metadata_store_->upsert_file_stub(stub);
  }
  // Create a task in the metadata store
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", test_file_path.string());
  EXPECT_GT(task_id, 0);
  
  // Set up mock expectations
  ExtractionResult mock_extraction_result;
  mock_extraction_result.content_hash = "test_content_hash_123";
  mock_extraction_result.chunks = MockUtilities::create_test_chunks(2, "Test chunk");
  
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .Times(1)
      .WillOnce(Return(mock_extraction_result));
  
  // Worker does not call get_file_type(); only extract_with_hash is used
  
  // Set up embedding expectations for each chunk
  std::vector<float> test_embedding = MockUtilities::create_test_embedding();
  EXPECT_CALL(*mock_ollama_client_, get_embedding(_))
      .Times(2) // one per chunk
      .WillRepeatedly(Return(test_embedding));
  
  // Run the task
  bool result = worker_->run_one_task();
  
  // Verify the task was processed
  EXPECT_TRUE(result);
  
  // Verify the task status was updated to COMPLETED
  auto completed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::COMPLETED);
  EXPECT_FALSE(completed_tasks.empty());
  EXPECT_EQ(completed_tasks[0].target_path.value(), test_file_path.string());
  
  // Verify file metadata was created
  auto file_metadata = metadata_store_->get_file_metadata(test_file_path.string());
  ASSERT_TRUE(file_metadata.has_value());
  EXPECT_EQ(file_metadata->content_hash, "test_content_hash_123");
  EXPECT_EQ(file_metadata->processing_status, ProcessingStatus::PROCESSED);
  
  cleanup_test_file(test_file_path);
}

TEST_F(WorkerTest, RunOneTaskWithFailedExtraction) {
  // Create a test file on disk
  auto test_file_path = create_test_file("Test content");
  // Insert file stub required by worker
  {
    BasicFileMetadata stub = magic_tests::TestUtilities::create_test_basic_file_metadata(
        test_file_path.string(),
        "test_hash_fail_extraction",
        FileType::Text,
        static_cast<size_t>(std::filesystem::file_size(test_file_path)),
        ProcessingStatus::QUEUED);
    metadata_store_->upsert_file_stub(stub);
  }
  long long task_id2 = task_queue_repo_->create_file_process_task("PROCESS_FILE", test_file_path.string());
  EXPECT_GT(task_id2, 0);
  
  // Set up mock to throw exception during extraction
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  // Worker path throws
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .WillOnce(::testing::Throw(std::runtime_error("Extraction failed")));
  
  // Run the task
  bool result = worker_->run_one_task();
  
  // Verify the task was processed (returned true) but failed
  EXPECT_TRUE(result);
  
  // Verify the task status was updated to FAILED
  auto failed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::FAILED);
  EXPECT_FALSE(failed_tasks.empty());
  EXPECT_EQ(failed_tasks[0].target_path.value(), test_file_path.string());
  
  cleanup_test_file(test_file_path);
}

TEST_F(WorkerTest, RunOneTaskWithNonExistentFile) {
  // Create a task for a file that doesn't exist
  const std::string non_existent_path = "/path/to/nonexistent/file.txt";
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", non_existent_path);
  EXPECT_GT(task_id, 0);
  
  // Run the task
  bool result = worker_->run_one_task();
  
  // Verify the task was processed
  EXPECT_TRUE(result);
  
  // Verify the task status was updated to FAILED
  auto failed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::FAILED);
  EXPECT_FALSE(failed_tasks.empty());
  EXPECT_EQ(failed_tasks[0].target_path.value(), "/path/to/nonexistent/file.txt");
}

TEST_F(WorkerTest, WorkerLifecycle) {
  // Test that worker can be created and destroyed without issues
  EXPECT_NO_THROW({
    auto test_service_provider = std::make_shared<ServiceProvider>(
        metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_
    );
    auto test_worker = std::make_unique<Worker>(2, test_service_provider);
    
    // Worker should be able to check for tasks even if none exist
    bool result = test_worker->run_one_task();
    EXPECT_FALSE(result);
  });
}

TEST_F(WorkerTest, MultipleTasksProcessedSequentially) {
  // Create multiple test files
  auto test_file1 = create_test_file("Content for file 1");
  auto test_file2 = create_test_file("Content for file 2");
  // Insert stubs for both files
  {
    BasicFileMetadata stub1 = magic_tests::TestUtilities::create_test_basic_file_metadata(
        test_file1.string(),
        "test_hash",
        FileType::Text,
        static_cast<size_t>(std::filesystem::file_size(test_file1)),
        ProcessingStatus::QUEUED);
    metadata_store_->upsert_file_stub(stub1);
    BasicFileMetadata stub2 = magic_tests::TestUtilities::create_test_basic_file_metadata(
        test_file2.string(),
        "test_hash",
        FileType::Text,
        static_cast<size_t>(std::filesystem::file_size(test_file2)),
        ProcessingStatus::QUEUED);
    metadata_store_->upsert_file_stub(stub2);
  }
  // Verify stubs exist
  {
    auto s1 = metadata_store_->get_file_metadata(test_file1.string());
    auto s2 = metadata_store_->get_file_metadata(test_file2.string());
    ASSERT_TRUE(s1.has_value());
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s1->path, test_file1.string());
    EXPECT_EQ(s2->path, test_file2.string());
  }
  
  // Create tasks for both files
  long long task_id1 = task_queue_repo_->create_file_process_task("PROCESS_FILE", test_file1.string());
  long long task_id2 = task_queue_repo_->create_file_process_task("PROCESS_FILE", test_file2.string());
  
  // Set up mock expectations for both tasks
  ExtractionResult mock_extraction_result;
  mock_extraction_result.content_hash = "test_hash";
  mock_extraction_result.chunks = MockUtilities::create_test_chunks(1, "Test");
  
  std::vector<float> test_embedding = MockUtilities::create_test_embedding();
  
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(2)
      .WillRepeatedly(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .Times(2)
      .WillRepeatedly(Return(mock_extraction_result));
  
  // Worker does not call get_file_type(); remove expectation
  
  EXPECT_CALL(*mock_ollama_client_, get_embedding(_))
      .Times(2) // 2 tasks * 1 chunk each
      .WillRepeatedly(Return(test_embedding));
  
  // Process both tasks
  bool result1 = worker_->run_one_task();
  bool result2 = worker_->run_one_task();
  
  EXPECT_TRUE(result1);
  EXPECT_TRUE(result2);
  
  // Verify both tasks were completed
  auto completed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::COMPLETED);
  EXPECT_EQ(completed_tasks.size(), 2);
  
  // Check that both file paths are in the completed tasks
  std::set<std::string> completed_paths;
  for (const auto& task : completed_tasks) {
    completed_paths.insert(task.target_path.value());
  }
  EXPECT_TRUE(completed_paths.count(test_file1.string()));
  EXPECT_TRUE(completed_paths.count(test_file2.string()));
  
  // No more tasks should be available
  bool result3 = worker_->run_one_task();
  EXPECT_FALSE(result3);
  
  cleanup_test_file(test_file1);
  cleanup_test_file(test_file2);
}

} // namespace magic_tests
