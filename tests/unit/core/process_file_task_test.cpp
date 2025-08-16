#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "magic_core/async/process_file_task.hpp"
#include "magic_core/async/service_provider.hpp"
#include "../../common/mocks_test.hpp"
#include "../../common/utilities_test.hpp"

namespace magic_tests {

using namespace magic_core;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::Throw;

class ProcessFileTaskTest : public MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    
    // Create mock dependencies
    mock_ollama_client_ = std::make_shared<StrictMock<MockOllamaClient>>();
    mock_content_extractor_factory_ = std::make_shared<StrictMock<MockContentExtractorFactory>>();
    mock_content_extractor_ = std::make_unique<StrictMock<MockContentExtractor>>();
    
    // Create service provider
    service_provider_ = std::make_shared<ServiceProvider>(
        metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_
    );
    
    // Setup progress callback to track progress updates
    progress_updates_.clear();
    progress_callback_ = [this](float progress, const std::string& message) {
      progress_updates_.emplace_back(progress, message);
    };
  }

  void TearDown() override {
    mock_content_extractor_.reset();
    mock_content_extractor_factory_.reset();
    mock_ollama_client_.reset();
    service_provider_.reset();
    MetadataStoreTestBase::TearDown();
  }

  ProcessFileTask create_test_task(const std::string& file_path, long long task_id = 1) {
    auto now = std::chrono::system_clock::now();
    return ProcessFileTask(task_id, TaskStatus::PENDING, now, now, std::nullopt, file_path);
  }

  // Helper method to create a test file on disk
  std::filesystem::path create_test_file(const std::string& content = "Test file content") {
    auto temp_file = std::filesystem::temp_directory_path() / ("test_file_" + std::to_string(std::rand()) + ".txt");
    std::ofstream file(temp_file);
    file << content;
    file.close();
    return temp_file;
  }

  void cleanup_test_file(const std::filesystem::path& file_path) {
    if (std::filesystem::exists(file_path)) {
      std::filesystem::remove(file_path);
    }
  }

  std::shared_ptr<ServiceProvider> service_provider_;
  std::shared_ptr<StrictMock<MockOllamaClient>> mock_ollama_client_;
  std::shared_ptr<StrictMock<MockContentExtractorFactory>> mock_content_extractor_factory_;
  std::unique_ptr<StrictMock<MockContentExtractor>> mock_content_extractor_;
  
  std::vector<std::pair<float, std::string>> progress_updates_;
  ProgressUpdater progress_callback_;
};

TEST_F(ProcessFileTaskTest, Constructor_InitializesCorrectly) {
  // Arrange
  std::string file_path = "/test/file.txt";
  auto now = std::chrono::system_clock::now();
  
  // Act
  ProcessFileTask task(123, TaskStatus::PENDING, now, now, std::nullopt, file_path);
  
  // Assert
  EXPECT_EQ(task.get_id(), 123);
  EXPECT_EQ(task.get_status(), TaskStatus::PENDING);
  EXPECT_STREQ(task.get_type(), "PROCESS_FILE");
  EXPECT_EQ(task.get_file_path(), file_path);
}

TEST_F(ProcessFileTaskTest, Execute_FileNotFoundInMetadata_ThrowsError) {
  // Arrange
  std::string non_existent_path = "/path/to/nonexistent/file.txt";
  ProcessFileTask task = create_test_task(non_existent_path);
  
  // Act & Assert
  EXPECT_THROW({
    task.execute(*service_provider_, progress_callback_);
  }, std::runtime_error);
  
  // Verify progress was called at least once
  EXPECT_FALSE(progress_updates_.empty());
  EXPECT_EQ(progress_updates_[0].first, 0.0f);
  EXPECT_EQ(progress_updates_[0].second, "Starting processing...");
}

TEST_F(ProcessFileTaskTest, Execute_SuccessfulProcessing_CompletesAllSteps) {
  // Arrange
  auto test_file_path = create_test_file("This is test content for processing.");
  
  // Create file metadata in store
  BasicFileMetadata stub = TestUtilities::create_test_basic_file_metadata(
      test_file_path.string(),
      "test_content_hash_123",
      FileType::Text,
      static_cast<size_t>(std::filesystem::file_size(test_file_path)),
      ProcessingStatus::QUEUED);
  metadata_store_->upsert_file_stub(stub);
  
  ProcessFileTask task = create_test_task(test_file_path.string());
  
  // Setup mock expectations
  ExtractionResult mock_extraction_result;
  mock_extraction_result.content_hash = "test_content_hash_123";
  mock_extraction_result.chunks = MockUtilities::create_test_chunks(2, "Test chunk");
  
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .Times(1)
      .WillOnce(Return(mock_extraction_result));
  
  std::vector<float> test_embedding = MockUtilities::create_test_embedding();
  EXPECT_CALL(*mock_ollama_client_, get_embedding(_))
      .Times(2) // one per chunk
      .WillRepeatedly(Return(test_embedding));
  
  // Act
  EXPECT_NO_THROW({
    task.execute(*service_provider_, progress_callback_);
  });
  
  // Assert
  EXPECT_FALSE(progress_updates_.empty());
  
  // Verify progress sequence
  EXPECT_EQ(progress_updates_[0].first, 0.0f);
  EXPECT_EQ(progress_updates_[0].second, "Starting processing...");
  
  EXPECT_EQ(progress_updates_[1].first, 0.05f);
  EXPECT_EQ(progress_updates_[1].second, "File metadata loaded.");
  
  EXPECT_EQ(progress_updates_[2].first, 0.1f);
  EXPECT_EQ(progress_updates_[2].second, "Content extracted.");
  
  // Should have chunk processing progress updates
  bool found_chunk_progress = false;
  for (const auto& update : progress_updates_) {
    if (update.second.find("Embedding chunk") != std::string::npos) {
      found_chunk_progress = true;
      break;
    }
  }
  EXPECT_TRUE(found_chunk_progress);
  
  // Final progress updates
  auto& final_updates = progress_updates_;
  EXPECT_EQ(final_updates[final_updates.size()-2].first, 0.95f);
  EXPECT_EQ(final_updates[final_updates.size()-2].second, "Document summary embedding stored.");
  
  EXPECT_EQ(final_updates[final_updates.size()-1].first, 1.0f);
  EXPECT_EQ(final_updates[final_updates.size()-1].second, "Processing complete.");
  
  cleanup_test_file(test_file_path);
}

TEST_F(ProcessFileTaskTest, Execute_ExtractorThrowsException_PropagatesError) {
  // Arrange
  auto test_file_path = create_test_file("Test content");
  
  BasicFileMetadata stub = TestUtilities::create_test_basic_file_metadata(
      test_file_path.string(),
      "test_hash",
      FileType::Text,
      static_cast<size_t>(std::filesystem::file_size(test_file_path)),
      ProcessingStatus::QUEUED);
  metadata_store_->upsert_file_stub(stub);
  
  ProcessFileTask task = create_test_task(test_file_path.string());
  
  // Setup mock to throw exception
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .WillOnce(Throw(std::runtime_error("Extraction failed")));
  
  // Act & Assert
  EXPECT_THROW({
    task.execute(*service_provider_, progress_callback_);
  }, std::runtime_error);
  
  cleanup_test_file(test_file_path);
}

TEST_F(ProcessFileTaskTest, Execute_OllamaClientThrowsException_PropagatesError) {
  // Arrange
  auto test_file_path = create_test_file("Test content");
  
  BasicFileMetadata stub = TestUtilities::create_test_basic_file_metadata(
      test_file_path.string(),
      "test_hash",
      FileType::Text,
      static_cast<size_t>(std::filesystem::file_size(test_file_path)),
      ProcessingStatus::QUEUED);
  metadata_store_->upsert_file_stub(stub);
  
  ProcessFileTask task = create_test_task(test_file_path.string());
  
  // Setup mocks
  ExtractionResult mock_extraction_result;
  mock_extraction_result.content_hash = "test_hash";
  mock_extraction_result.chunks = MockUtilities::create_test_chunks(1, "Test chunk");
  
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .Times(1)
      .WillOnce(Return(mock_extraction_result));
  
  // Ollama client throws exception
  EXPECT_CALL(*mock_ollama_client_, get_embedding(_))
      .Times(1)
      .WillOnce(Throw(std::runtime_error("Ollama service unavailable")));
  
  // Act & Assert
  EXPECT_THROW({
    task.execute(*service_provider_, progress_callback_);
  }, std::runtime_error);
  
  cleanup_test_file(test_file_path);
}

TEST_F(ProcessFileTaskTest, Execute_EmptyEmbeddingFromOllama_ThrowsError) {
  // Arrange
  auto test_file_path = create_test_file("Test content");
  
  BasicFileMetadata stub = TestUtilities::create_test_basic_file_metadata(
      test_file_path.string(),
      "test_hash",
      FileType::Text,
      static_cast<size_t>(std::filesystem::file_size(test_file_path)),
      ProcessingStatus::QUEUED);
  metadata_store_->upsert_file_stub(stub);
  
  ProcessFileTask task = create_test_task(test_file_path.string());
  
  // Setup mocks
  ExtractionResult mock_extraction_result;
  mock_extraction_result.content_hash = "test_hash";
  mock_extraction_result.chunks = MockUtilities::create_test_chunks(1, "Test chunk");
  
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .Times(1)
      .WillOnce(Return(mock_extraction_result));
  
  // Return empty embedding
  std::vector<float> empty_embedding;
  EXPECT_CALL(*mock_ollama_client_, get_embedding(_))
      .Times(1)
      .WillOnce(Return(empty_embedding));
  
  // Act & Assert
  EXPECT_THROW({
    task.execute(*service_provider_, progress_callback_);
  }, std::runtime_error);
  
  cleanup_test_file(test_file_path);
}

TEST_F(ProcessFileTaskTest, Execute_NoChunksFromExtraction_CompletesSuccessfully) {
  // Arrange
  auto test_file_path = create_test_file("");
  
  BasicFileMetadata stub = TestUtilities::create_test_basic_file_metadata(
      test_file_path.string(),
      "empty_file_hash",
      FileType::Text,
      0,
      ProcessingStatus::QUEUED);
  metadata_store_->upsert_file_stub(stub);
  
  ProcessFileTask task = create_test_task(test_file_path.string());
  
  // Setup mocks for empty file
  ExtractionResult mock_extraction_result;
  mock_extraction_result.content_hash = "empty_file_hash";
  mock_extraction_result.chunks = {}; // Empty chunks
  
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .Times(1)
      .WillOnce(Return(mock_extraction_result));
  
  // No embedding calls expected for empty file
  
  // Act
  EXPECT_NO_THROW({
    task.execute(*service_provider_, progress_callback_);
  });
  
  // Assert - Should complete successfully
  EXPECT_FALSE(progress_updates_.empty());
  EXPECT_EQ(progress_updates_.back().first, 1.0f);
  EXPECT_EQ(progress_updates_.back().second, "Processing complete.");
  
  cleanup_test_file(test_file_path);
}

TEST_F(ProcessFileTaskTest, Execute_LargeNumberOfChunks_ProcessesInBatches) {
  // Arrange
  auto test_file_path = create_test_file("Large file content");
  
  BasicFileMetadata stub = TestUtilities::create_test_basic_file_metadata(
      test_file_path.string(),
      "large_file_hash",
      FileType::Text,
      static_cast<size_t>(std::filesystem::file_size(test_file_path)),
      ProcessingStatus::QUEUED);
  metadata_store_->upsert_file_stub(stub);
  
  ProcessFileTask task = create_test_task(test_file_path.string());
  
  // Create many chunks to test batching (more than batch size of 64)
  ExtractionResult mock_extraction_result;
  mock_extraction_result.content_hash = "large_file_hash";
  mock_extraction_result.chunks = MockUtilities::create_test_chunks(100, "Test chunk");
  
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(_))
      .Times(1)
      .WillOnce(Return(mock_extraction_result));
  
  std::vector<float> test_embedding = MockUtilities::create_test_embedding();
  EXPECT_CALL(*mock_ollama_client_, get_embedding(_))
      .Times(100) // one per chunk
      .WillRepeatedly(Return(test_embedding));
  
  // Act
  EXPECT_NO_THROW({
    task.execute(*service_provider_, progress_callback_);
  });
  
  // Assert - Should have many progress updates for chunks
  int chunk_progress_count = 0;
  for (const auto& update : progress_updates_) {
    if (update.second.find("Embedding chunk") != std::string::npos) {
      chunk_progress_count++;
    }
  }
  EXPECT_EQ(chunk_progress_count, 100);
  
  cleanup_test_file(test_file_path);
}

TEST_F(ProcessFileTaskTest, GetType_ReturnsCorrectType) {
  // Arrange
  ProcessFileTask task = create_test_task("/test/file.txt");
  
  // Act & Assert
  EXPECT_STREQ(task.get_type(), "PROCESS_FILE");
}

TEST_F(ProcessFileTaskTest, GetFilePath_ReturnsCorrectPath) {
  // Arrange
  std::string file_path = "/test/specific/file.txt";
  ProcessFileTask task = create_test_task(file_path);
  
  // Act & Assert
  EXPECT_EQ(task.get_file_path(), file_path);
}

} // namespace magic_tests
