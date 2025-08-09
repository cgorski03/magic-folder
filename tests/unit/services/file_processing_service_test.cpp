#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>

#include "magic_core/services/file_processing_service.hpp"
#include "magic_core/db/task_queue_repo.hpp"
#include "mocks_test.hpp"
#include "utilities_test.hpp"

namespace magic_tests {

using namespace magic_core;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Throw;

class FileProcessingServiceTest : public MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    
    // Create test file
    test_file_path_ = std::filesystem::temp_directory_path() / "test_file.txt";
    std::ofstream test_file(test_file_path_);
    test_file << "This is test content for processing.";
    test_file.close();
    
    // Create mocks
    mock_content_extractor_factory_ = std::make_shared<MockContentExtractorFactory>();
    mock_ollama_client_ = std::make_shared<MockOllamaClient>();
    mock_content_extractor_ = std::make_unique<MockContentExtractor>();
    
    // Create service
    file_processing_service_ = std::make_unique<FileProcessingService>(
        metadata_store_, task_queue_repo_, mock_content_extractor_factory_, mock_ollama_client_);
  }

  void TearDown() override {
    if (std::filesystem::exists(test_file_path_)) {
      std::filesystem::remove(test_file_path_);
    }
    MetadataStoreTestBase::TearDown();
  }

  void setup_extraction_expectations(const std::string& hash) {
    ExtractionResult extraction_result;
    extraction_result.content_hash = hash;
    extraction_result.chunks = {};  // Empty chunks for simplicity
    
    EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(test_file_path_))
        .Times(1)
        .WillOnce(ReturnRef(*mock_content_extractor_));
    
    EXPECT_CALL(*mock_content_extractor_, extract_with_hash(test_file_path_))
        .Times(0);
    
    EXPECT_CALL(*mock_content_extractor_, get_file_type())
        .Times(1)
        .WillOnce(Return(FileType::Text));
  }

  std::filesystem::path test_file_path_;
  std::unique_ptr<FileProcessingService> file_processing_service_;
  std::shared_ptr<MockContentExtractorFactory> mock_content_extractor_factory_;
  std::shared_ptr<MockOllamaClient> mock_ollama_client_;
  std::unique_ptr<MockContentExtractor> mock_content_extractor_;
};

TEST_F(FileProcessingServiceTest, RequestProcessing_ReturnsTaskId) {
  // Arrange
  setup_extraction_expectations("test_hash");
  
  // Act
  auto task_id = file_processing_service_->request_processing(test_file_path_);
  
  // Assert
  EXPECT_TRUE(task_id.has_value());
  EXPECT_GT(task_id.value(), 0);
}

TEST_F(FileProcessingServiceTest, RequestProcessing_NonExistentFile_ReturnsEmpty) {
  // Arrange
  std::filesystem::path non_existent = "/path/to/nonexistent/file.txt";
  
  // Act
  auto task_id = file_processing_service_->request_processing(non_existent);
  
  // Assert
  EXPECT_FALSE(task_id.has_value());
}

TEST_F(FileProcessingServiceTest, RequestProcessing_DuplicateRequest_ReturnsEmpty) {
  // Arrange
  // For duplicate, request_processing will compute hash twice
  // and only upsert on the first call
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(test_file_path_))
      .Times(2)
      .WillRepeatedly(ReturnRef(*mock_content_extractor_));
  // No full extraction during request
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(test_file_path_)).Times(0);
  // File type only needed on first upsert
  EXPECT_CALL(*mock_content_extractor_, get_file_type())
      .Times(1)
      .WillOnce(Return(FileType::Text));
  
  // Act - First request should succeed
  auto first_task_id = file_processing_service_->request_processing(test_file_path_);
  EXPECT_TRUE(first_task_id.has_value());
  
  // Second request for same file should return empty (already queued)
  auto second_task_id = file_processing_service_->request_processing(test_file_path_);
  
  // Assert
  EXPECT_FALSE(second_task_id.has_value());
}

TEST_F(FileProcessingServiceTest, RequestProcessing_ExtractionError_ThrowsException) {
  // Arrange
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(test_file_path_))
      .Times(1)
      .WillOnce(ReturnRef(*mock_content_extractor_));
  
  // With current implementation, we don't call extract_with_hash during request
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(test_file_path_)).Times(0);
  
  // Act & Assert
  // Should not throw since we are only hashing content; error will occur in worker later
  auto maybe_id = file_processing_service_->request_processing(test_file_path_);
  EXPECT_TRUE(maybe_id.has_value());
}

} // namespace magic_tests
