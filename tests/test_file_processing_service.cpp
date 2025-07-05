#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

#include "magic_core/content_extractor.hpp"
#include "magic_core/metadata_store.hpp"
#include "magic_core/ollama_client.hpp"
#include "magic_services/file_processing_service.hpp"
#include "test_mocks.hpp"
#include "test_utilities.hpp"

namespace magic_services {

class FileProcessingServiceTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    // Call parent setup to initialize metadata_store_
    MetadataStoreTestBase::SetUp();

    // Create mock objects
    mock_content_extractor_ = std::make_shared<magic_tests::MockContentExtractor>();
    mock_ollama_client_ = std::make_shared<magic_tests::MockOllamaClient>();

    // Create the service with mocked dependencies
    file_processing_service_ = std::make_unique<FileProcessingService>(
        metadata_store_, mock_content_extractor_, mock_ollama_client_);

    // Set up test file path and create test file
    setupTestFile();
  }

  void TearDown() override {
    // Clean up test file
    if (std::filesystem::exists(test_file_path_)) {
      std::filesystem::remove(test_file_path_);
    }

    MetadataStoreTestBase::TearDown();
  }

  // Helper method to create a test ExtractedContent
  magic_core::ExtractedContent create_test_extracted_content() {
    return magic_tests::MockUtilities::create_test_extracted_content();
  }

  // Helper method to create a test embedding
  std::vector<float> create_test_embedding() {
    return magic_tests::MockUtilities::create_test_embedding();
  }

  // Set up test file for processing
  void setupTestFile() {
    test_file_path_ = std::filesystem::temp_directory_path() / "test_file.txt";

    // Create a test file
    std::ofstream test_file(test_file_path_);
    test_file << "This is test content for processing.";
    test_file.close();
  }

  // Helper method to set up mock expectations for successful processing
  void setupSuccessfulProcessingExpectations(const magic_core::ExtractedContent& content,
                                             const std::vector<float>& embedding) {
    EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
        .WillOnce(testing::Return(content));

    EXPECT_CALL(*mock_ollama_client_, get_embedding(content.text_content))
        .WillOnce(testing::Return(embedding));
  }

  std::unique_ptr<FileProcessingService> file_processing_service_;
  std::shared_ptr<magic_tests::MockContentExtractor> mock_content_extractor_;
  std::shared_ptr<magic_tests::MockOllamaClient> mock_ollama_client_;
  std::filesystem::path test_file_path_;
};

// Test successful file processing
TEST_F(FileProcessingServiceTest, ProcessFile_SuccessfulProcessing) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  auto embedding = create_test_embedding();

  setupSuccessfulProcessingExpectations(extracted_content, embedding);

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());

  EXPECT_EQ(stored_metadata->path, test_file_path_);
  EXPECT_EQ(stored_metadata->content_hash, "test_hash_123");
  EXPECT_EQ(stored_metadata->file_type, magic_core::FileType::Text);
  EXPECT_EQ(stored_metadata->file_size, std::filesystem::file_size(test_file_path_));
  EXPECT_EQ(stored_metadata->vector_embedding, embedding);
  EXPECT_EQ(stored_metadata->vector_embedding.size(), 768);
}

// Test file processing with different file types
TEST_F(FileProcessingServiceTest, ProcessFile_MarkdownFile) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  extracted_content.file_type = magic_core::FileType::Markdown;
  extracted_content.title = "Test Markdown";
  auto embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
      .WillOnce(testing::Return(embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->file_type, magic_core::FileType::Markdown);
}

// Test file processing with code file
TEST_F(FileProcessingServiceTest, ProcessFile_CodeFile) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  extracted_content.file_type = magic_core::FileType::Code;
  auto embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
      .WillOnce(testing::Return(embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->file_type, magic_core::FileType::Code);
}

// Test that processing updates existing metadata
TEST_F(FileProcessingServiceTest, ProcessFile_UpdatesExistingMetadata) {
  // Arrange - First processing
  auto first_content = create_test_extracted_content();
  auto first_embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(first_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(first_content.text_content))
      .WillOnce(testing::Return(first_embedding));

  file_processing_service_->process_file(test_file_path_);

  // Arrange - Second processing with different content
  auto second_content = create_test_extracted_content();
  second_content.text_content = "Updated content for processing.";
  second_content.content_hash = "updated_hash_456";
  auto second_embedding = create_test_embedding();
  second_embedding[0] = 0.9f;  // Different embedding

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(second_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(second_content.text_content))
      .WillOnce(testing::Return(second_embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->content_hash, "updated_hash_456");
  EXPECT_EQ(stored_metadata->vector_embedding[0], 0.9f);
}

// Test error handling when content extraction fails
TEST_F(FileProcessingServiceTest, ProcessFile_ContentExtractionError) {
  // Arrange
  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Throw(magic_core::ContentExtractorError("Extraction failed")));

  // Act & Assert
  EXPECT_THROW(file_processing_service_->process_file(test_file_path_),
               magic_core::ContentExtractorError);
}

// Test error handling when Ollama embedding fails
TEST_F(FileProcessingServiceTest, ProcessFile_OllamaEmbeddingError) {
  // Arrange
  auto extracted_content = create_test_extracted_content();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
      .WillOnce(testing::Throw(magic_core::OllamaError("Embedding failed")));

  // Act & Assert
  EXPECT_THROW(file_processing_service_->process_file(test_file_path_), magic_core::OllamaError);
}

// Test processing with empty content
TEST_F(FileProcessingServiceTest, ProcessFile_EmptyContent) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  extracted_content.text_content = "";
  extracted_content.word_count = 0;
  auto embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding("")).WillOnce(testing::Return(embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->content_hash, "test_hash_123");
}

// Test processing with large content
TEST_F(FileProcessingServiceTest, ProcessFile_LargeContent) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  extracted_content.text_content = std::string(10000, 'a');  // Large content
  extracted_content.word_count = 10000;
  auto embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(testing::StrEq(std::string(10000, 'a'))))
      .WillOnce(testing::Return(embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->content_hash, "test_hash_123");
}

// Test processing with special characters in content
TEST_F(FileProcessingServiceTest, ProcessFile_SpecialCharacters) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  extracted_content.text_content = "Content with special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?";
  auto embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
      .WillOnce(testing::Return(embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->content_hash, "test_hash_123");
}

// Test processing with unicode content
TEST_F(FileProcessingServiceTest, ProcessFile_UnicodeContent) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  extracted_content.text_content = "Unicode content: 🚀🌟🎉 你好世界 Привет мир";
  auto embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
      .WillOnce(testing::Return(embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->content_hash, "test_hash_123");
}

// Test that timestamps are properly set
TEST_F(FileProcessingServiceTest, ProcessFile_TimestampsAreSet) {
  // Arrange
  auto extracted_content = create_test_extracted_content();
  auto embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(extracted_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
      .WillOnce(testing::Return(embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());

  // Check that timestamps are set (not default values)
  auto now = std::chrono::system_clock::now();
  auto time_diff_modified =
      std::chrono::duration_cast<std::chrono::seconds>(now - stored_metadata->last_modified)
          .count();
  auto time_diff_created =
      std::chrono::duration_cast<std::chrono::seconds>(now - stored_metadata->created_at).count();

  // Timestamps should be within the last hour
  EXPECT_LE(std::abs(time_diff_modified), 3600);
  EXPECT_LE(std::abs(time_diff_created), 3600);
}

// Test processing multiple files
TEST_F(FileProcessingServiceTest, ProcessFile_MultipleFiles) {
  // Create second test file
  auto second_file_path = std::filesystem::temp_directory_path() / "test_file2.txt";
  std::ofstream second_file(second_file_path);
  second_file << "Second test file content.";
  second_file.close();

  // Arrange for first file
  auto first_content = create_test_extracted_content();
  auto first_embedding = create_test_embedding();

  EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
      .WillOnce(testing::Return(first_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(first_content.text_content))
      .WillOnce(testing::Return(first_embedding));

  // Arrange for second file
  auto second_content = create_test_extracted_content();
  second_content.text_content = "Second test file content.";
  second_content.content_hash = "second_hash_789";
  auto second_embedding = create_test_embedding();
  second_embedding[0] = 0.8f;

  EXPECT_CALL(*mock_content_extractor_, extract_content(second_file_path))
      .WillOnce(testing::Return(second_content));

  EXPECT_CALL(*mock_ollama_client_, get_embedding(second_content.text_content))
      .WillOnce(testing::Return(second_embedding));

  // Act
  file_processing_service_->process_file(test_file_path_);
  file_processing_service_->process_file(second_file_path);

  // Assert
  auto first_metadata = metadata_store_->get_file_metadata(test_file_path_);
  auto second_metadata = metadata_store_->get_file_metadata(second_file_path);

  ASSERT_TRUE(first_metadata.has_value());
  ASSERT_TRUE(second_metadata.has_value());

  EXPECT_EQ(first_metadata->content_hash, "test_hash_123");
  EXPECT_EQ(second_metadata->content_hash, "second_hash_789");
  EXPECT_EQ(first_metadata->vector_embedding[0], 0.5f);
  EXPECT_EQ(second_metadata->vector_embedding[0], 0.8f);

  // Clean up
  std::filesystem::remove(second_file_path);
}

}  // namespace magic_services