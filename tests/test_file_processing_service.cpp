#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>
#include <regex>
#include <iostream>

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
  EXPECT_EQ(stored_metadata->vector_embedding.size(), 1024);
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

/*
Content Hashing Tests
*/
TEST_F(FileProcessingServiceTest, ProcessFile_ContentHashComputation) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  // Create service with real ContentExtractor and mock OllamaClient
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Create test file with known content
  std::string test_content = "This is test content for hash computation.";
  std::ofstream test_file(test_file_path_);
  test_file << test_content;
  test_file.close();

  // Set up mock expectation for embedding
  auto embedding = create_test_embedding();
  EXPECT_CALL(*mock_ollama_client, get_embedding(test_content))
      .WillOnce(testing::Return(embedding));

  // Act
  real_file_processing_service->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  
  // Verify that content_hash is not empty and is a proper SHA256 hash
  EXPECT_FALSE(stored_metadata->content_hash.empty());
  EXPECT_EQ(stored_metadata->content_hash.length(), 64);  // SHA256 hash is 64 hex chars
  
  // Verify it's a valid hex string
  std::regex hex_regex("^[0-9a-f]{64}$");
  EXPECT_TRUE(std::regex_match(stored_metadata->content_hash, hex_regex));
}

// Test that different content produces different hashes - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_DifferentContentDifferentHashes) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Create first test file
  std::string first_content = "First test content for hash comparison.";
  std::ofstream first_file(test_file_path_);
  first_file << first_content;
  first_file.close();

  // Create second test file with different content
  auto second_file_path = std::filesystem::temp_directory_path() / "test_file2.txt";
  std::string second_content = "Second test content for hash comparison.";
  std::ofstream second_file(second_file_path);
  second_file << second_content;
  second_file.close();

  // Set up mock expectations for embeddings
  auto first_embedding = create_test_embedding();
  auto second_embedding = create_test_embedding();
  second_embedding[0] = 0.8f;  // Different embedding

  EXPECT_CALL(*mock_ollama_client, get_embedding(first_content))
      .WillOnce(testing::Return(first_embedding));
  EXPECT_CALL(*mock_ollama_client, get_embedding(second_content))
      .WillOnce(testing::Return(second_embedding));

  // Act
  real_file_processing_service->process_file(test_file_path_);
  real_file_processing_service->process_file(second_file_path);

  // Assert
  auto first_metadata = metadata_store_->get_file_metadata(test_file_path_);
  auto second_metadata = metadata_store_->get_file_metadata(second_file_path);

  ASSERT_TRUE(first_metadata.has_value());
  ASSERT_TRUE(second_metadata.has_value());

  // Verify hashes are different
  EXPECT_NE(first_metadata->content_hash, second_metadata->content_hash);
  EXPECT_FALSE(first_metadata->content_hash.empty());
  EXPECT_FALSE(second_metadata->content_hash.empty());

  // Verify hash format
  std::regex hex_regex("^[0-9a-f]{64}$");
  bool first_hash_valid = std::regex_match(first_metadata->content_hash, hex_regex);
  bool second_hash_valid = std::regex_match(second_metadata->content_hash, hex_regex);
  
  EXPECT_TRUE(first_hash_valid);
  EXPECT_TRUE(second_hash_valid);

  // Clean up
  std::filesystem::remove(second_file_path);
}

// Test that identical content produces identical hashes - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_IdenticalContentIdenticalHashes) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Create two files with identical content
  std::string identical_content = "Identical content for hash verification.";
  
  std::ofstream first_file(test_file_path_);
  first_file << identical_content;
  first_file.close();

  auto second_file_path = std::filesystem::temp_directory_path() / "test_file2.txt";
  std::ofstream second_file(second_file_path);
  second_file << identical_content;
  second_file.close();

  // Set up mock expectations for embeddings (same content, same embedding)
  auto embedding = create_test_embedding();
  EXPECT_CALL(*mock_ollama_client, get_embedding(identical_content))
      .WillOnce(testing::Return(embedding))
      .WillOnce(testing::Return(embedding));  // Called twice for identical content

  // Act
  real_file_processing_service->process_file(test_file_path_);
  real_file_processing_service->process_file(second_file_path);

  // Assert
  auto first_metadata = metadata_store_->get_file_metadata(test_file_path_);
  auto second_metadata = metadata_store_->get_file_metadata(second_file_path);

  ASSERT_TRUE(first_metadata.has_value());
  ASSERT_TRUE(second_metadata.has_value());

  // Verify hashes are identical
  EXPECT_EQ(first_metadata->content_hash, second_metadata->content_hash);
  EXPECT_FALSE(first_metadata->content_hash.empty());

  // Clean up
  std::filesystem::remove(second_file_path);
}

// Test that content hash computation works with different file types - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_ContentHashDifferentFileTypes) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Create different file types with same content
  std::string test_content = "Test content for different file types.";
  
  // Text file
  std::ofstream text_file(test_file_path_);
  text_file << test_content;
  text_file.close();

  // Markdown file
  auto md_file_path = std::filesystem::temp_directory_path() / "test_file.md";
  std::ofstream md_file(md_file_path);
  md_file << test_content;
  md_file.close();

  // Code file
  auto code_file_path = std::filesystem::temp_directory_path() / "test_file.cpp";
  std::ofstream code_file(code_file_path);
  code_file << test_content;
  code_file.close();

  // Set up mock expectations for embeddings (same content, same embedding)
  auto embedding = create_test_embedding();
  EXPECT_CALL(*mock_ollama_client, get_embedding(test_content))
      .WillOnce(testing::Return(embedding))
      .WillOnce(testing::Return(embedding))
      .WillOnce(testing::Return(embedding));  // Called three times for three files

  // Act
  real_file_processing_service->process_file(test_file_path_);
  real_file_processing_service->process_file(md_file_path);
  real_file_processing_service->process_file(code_file_path);

  // Assert
  auto text_metadata = metadata_store_->get_file_metadata(test_file_path_);
  auto md_metadata = metadata_store_->get_file_metadata(md_file_path);
  auto code_metadata = metadata_store_->get_file_metadata(code_file_path);

  ASSERT_TRUE(text_metadata.has_value());
  ASSERT_TRUE(md_metadata.has_value());
  ASSERT_TRUE(code_metadata.has_value());

  // All should have the same hash since content is identical
  EXPECT_EQ(text_metadata->content_hash, md_metadata->content_hash);
  EXPECT_EQ(md_metadata->content_hash, code_metadata->content_hash);
  EXPECT_FALSE(text_metadata->content_hash.empty());

  // Clean up
  std::filesystem::remove(md_file_path);
  std::filesystem::remove(code_file_path);
}

// Test that real ContentExtractor produces real hashes - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_RealContentExtractorIgnoresMockHash) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Create test file with content that would produce a different hash than the mock
  std::string test_content = "This content will produce a real hash, not the mock hash.";
  std::ofstream test_file(test_file_path_);
  test_file << test_content;
  test_file.close();

  // Set up mock expectation for embedding
  auto embedding = create_test_embedding();
  EXPECT_CALL(*mock_ollama_client, get_embedding(test_content))
      .WillOnce(testing::Return(embedding));

  // Act
  real_file_processing_service->process_file(test_file_path_);

  // Assert
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());

  // Verify the hash is NOT the mock hash
  EXPECT_NE(stored_metadata->content_hash, "test_hash_123");
  EXPECT_FALSE(stored_metadata->content_hash.empty());
  
  // Verify it's a proper SHA256 hash
  std::regex hex_regex("^[0-9a-f]{64}$");
  EXPECT_TRUE(std::regex_match(stored_metadata->content_hash, hex_regex));
}

// Test hash computation with very small files (1-10 bytes) - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_HashComputationSmallFiles) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Test files with different small sizes
  std::vector<std::pair<std::string, std::string>> small_files = {
    {"a", "Single character file"},
    {"ab", "Two character file"},
    {"abc", "Three character file"},
    {"abcd", "Four character file"},
    {"abcde", "Five character file"},
    {"abcdefgh", "Eight character file"},
    {"abcdefghij", "Ten character file"}
  };

  std::vector<std::string> computed_hashes;
  
  for (size_t i = 0; i < small_files.size(); ++i) {
    auto file_path = std::filesystem::temp_directory_path() / ("small_file_" + std::to_string(i) + ".txt");
    
    // Create file with content
    std::ofstream file(file_path);
    file << small_files[i].first;
    file.close();

    std::cout << "=== SMALL FILE TEST " << i << " ===" << std::endl;
    std::cout << "Content: '" << small_files[i].first << "'" << std::endl;
    std::cout << "File size: " << std::filesystem::file_size(file_path) << std::endl;

    // Set up mock expectation for embedding
    auto embedding = create_test_embedding();
    EXPECT_CALL(*mock_ollama_client, get_embedding(small_files[i].first))
        .WillOnce(testing::Return(embedding));

    // Process file
    real_file_processing_service->process_file(file_path);

    // Get metadata
    auto metadata = metadata_store_->get_file_metadata(file_path);
    ASSERT_TRUE(metadata.has_value());
    
    std::cout << "Computed hash: " << metadata->content_hash << std::endl;
    std::cout << "Hash length: " << metadata->content_hash.length() << std::endl;
    
    // Verify hash is valid
    EXPECT_FALSE(metadata->content_hash.empty());
    EXPECT_EQ(metadata->content_hash.length(), 64);
    
    std::regex hex_regex("^[0-9a-f]{64}$");
    EXPECT_TRUE(std::regex_match(metadata->content_hash, hex_regex));
    
    // Store hash for comparison
    computed_hashes.push_back(metadata->content_hash);
    
    // Clean up
    std::filesystem::remove(file_path);
  }

  // Verify all hashes are different (since content is different)
  for (size_t i = 0; i < computed_hashes.size(); ++i) {
    for (size_t j = i + 1; j < computed_hashes.size(); ++j) {
      EXPECT_NE(computed_hashes[i], computed_hashes[j]) 
          << "Hashes should be different for different content: '" 
          << small_files[i].first << "' vs '" << small_files[j].first << "'";
    }
  }
}

// Test hash computation with medium files (100-1000 bytes) - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_HashComputationMediumFiles) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Test files with medium sizes
  std::vector<std::pair<std::string, std::string>> medium_files = {
    {std::string(100, 'a'), "100 bytes of 'a'"},
    {std::string(500, 'b'), "500 bytes of 'b'"},
    {std::string(1000, 'c'), "1000 bytes of 'c'"},
    {"This is a medium-sized text file with some content that should be long enough to test hash computation properly. " + std::string(200, 'x'), "Text with 200 x's"},
    {std::string(750, 'z') + "END", "750 z's plus END"}
  };

  std::vector<std::string> computed_hashes;
  
  for (size_t i = 0; i < medium_files.size(); ++i) {
    auto file_path = std::filesystem::temp_directory_path() / ("medium_file_" + std::to_string(i) + ".txt");
    
    // Create file with content
    std::ofstream file(file_path);
    file << medium_files[i].first;
    file.close();

    std::cout << "=== MEDIUM FILE TEST " << i << " ===" << std::endl;
    std::cout << "Description: " << medium_files[i].second << std::endl;
    std::cout << "File size: " << std::filesystem::file_size(file_path) << std::endl;

    // Set up mock expectation for embedding
    auto embedding = create_test_embedding();
    EXPECT_CALL(*mock_ollama_client, get_embedding(medium_files[i].first))
        .WillOnce(testing::Return(embedding));

    // Process file
    real_file_processing_service->process_file(file_path);

    // Get metadata
    auto metadata = metadata_store_->get_file_metadata(file_path);
    ASSERT_TRUE(metadata.has_value());
    
    std::cout << "Computed hash: " << metadata->content_hash << std::endl;
    
    // Verify hash is valid
    EXPECT_FALSE(metadata->content_hash.empty());
    EXPECT_EQ(metadata->content_hash.length(), 64);
    
    std::regex hex_regex("^[0-9a-f]{64}$");
    EXPECT_TRUE(std::regex_match(metadata->content_hash, hex_regex));
    
    // Store hash for comparison
    computed_hashes.push_back(metadata->content_hash);
    
    // Clean up
    std::filesystem::remove(file_path);
  }

  // Verify all hashes are different
  for (size_t i = 0; i < computed_hashes.size(); ++i) {
    for (size_t j = i + 1; j < computed_hashes.size(); ++j) {
      EXPECT_NE(computed_hashes[i], computed_hashes[j]) 
          << "Hashes should be different for different content";
    }
  }
}

// Test hash computation with large files (>1024 bytes) - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_HashComputationLargeFiles) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Test files with large sizes (around and above the 1024-byte buffer size)
  std::vector<std::pair<std::string, std::string>> large_files = {
    {std::string(1023, 'x'), "1023 bytes (just under buffer)"},
    {std::string(1024, 'y'), "1024 bytes (exactly buffer size)"},
    {std::string(1025, 'z'), "1025 bytes (just over buffer)"},
    {std::string(2048, 'a'), "2048 bytes (exactly 2 buffers)"},
    {std::string(3000, 'b'), "3000 bytes (multiple buffers)"},
    {std::string(5000, 'c'), "5000 bytes (large file)"}
  };

  std::vector<std::string> computed_hashes;
  
  for (size_t i = 0; i < large_files.size(); ++i) {
    auto file_path = std::filesystem::temp_directory_path() / ("large_file_" + std::to_string(i) + ".txt");
    
    // Create file with content
    std::ofstream file(file_path);
    file << large_files[i].first;
    file.close();

    std::cout << "=== LARGE FILE TEST " << i << " ===" << std::endl;
    std::cout << "Description: " << large_files[i].second << std::endl;
    std::cout << "File size: " << std::filesystem::file_size(file_path) << std::endl;

    // Set up mock expectation for embedding
    auto embedding = create_test_embedding();
    EXPECT_CALL(*mock_ollama_client, get_embedding(large_files[i].first))
        .WillOnce(testing::Return(embedding));

    // Process file
    real_file_processing_service->process_file(file_path);

    // Get metadata
    auto metadata = metadata_store_->get_file_metadata(file_path);
    ASSERT_TRUE(metadata.has_value());
    
    std::cout << "Computed hash: " << metadata->content_hash << std::endl;
    
    // Verify hash is valid
    EXPECT_FALSE(metadata->content_hash.empty());
    EXPECT_EQ(metadata->content_hash.length(), 64);
    
    std::regex hex_regex("^[0-9a-f]{64}$");
    EXPECT_TRUE(std::regex_match(metadata->content_hash, hex_regex));
    
    // Store hash for comparison
    computed_hashes.push_back(metadata->content_hash);
    
    // Clean up
    std::filesystem::remove(file_path);
  }

  // Verify all hashes are different
  for (size_t i = 0; i < computed_hashes.size(); ++i) {
    for (size_t j = i + 1; j < computed_hashes.size(); ++j) {
      EXPECT_NE(computed_hashes[i], computed_hashes[j]) 
          << "Hashes should be different for different content";
    }
  }
}

// Test hash computation with edge case file sizes - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_HashComputationEdgeCases) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Test edge case file sizes
  std::vector<std::pair<std::string, std::string>> edge_cases = {
    {"", "Empty file"},
    {"a", "Single byte"},
    {std::string(1023, 'x'), "1023 bytes (just under buffer)"},
    {std::string(1024, 'y'), "1024 bytes (exactly buffer)"},
    {std::string(1025, 'z'), "1025 bytes (just over buffer)"},
    {std::string(2047, 'a'), "2047 bytes (just under 2 buffers)"},
    {std::string(2048, 'b'), "2048 bytes (exactly 2 buffers)"},
    {std::string(2049, 'c'), "2049 bytes (just over 2 buffers)"}
  };

  std::vector<std::string> computed_hashes;
  
  for (size_t i = 0; i < edge_cases.size(); ++i) {
    auto file_path = std::filesystem::temp_directory_path() / ("edge_case_" + std::to_string(i) + ".txt");
    
    // Create file with content
    std::ofstream file(file_path);
    file << edge_cases[i].first;
    file.close();

    std::cout << "=== EDGE CASE TEST " << i << " ===" << std::endl;
    std::cout << "Description: " << edge_cases[i].second << std::endl;
    std::cout << "File size: " << std::filesystem::file_size(file_path) << std::endl;

    // Set up mock expectation for embedding
    auto embedding = create_test_embedding();
    EXPECT_CALL(*mock_ollama_client, get_embedding(edge_cases[i].first))
        .WillOnce(testing::Return(embedding));

    // Process file
    real_file_processing_service->process_file(file_path);

    // Get metadata
    auto metadata = metadata_store_->get_file_metadata(file_path);
    ASSERT_TRUE(metadata.has_value());
    
    std::cout << "Computed hash: " << metadata->content_hash << std::endl;
    
    // Verify hash is valid
    EXPECT_FALSE(metadata->content_hash.empty());
    EXPECT_EQ(metadata->content_hash.length(), 64);
    
    std::regex hex_regex("^[0-9a-f]{64}$");
    EXPECT_TRUE(std::regex_match(metadata->content_hash, hex_regex));
    
    // Store hash for comparison
    computed_hashes.push_back(metadata->content_hash);
    
    // Clean up
    std::filesystem::remove(file_path);
  }

  // Verify all hashes are different (except empty file should have empty hash)
  for (size_t i = 0; i < computed_hashes.size(); ++i) {
    for (size_t j = i + 1; j < computed_hashes.size(); ++j) {
      if (i == 0 && j == 0) continue; // Skip empty file comparison
      EXPECT_NE(computed_hashes[i], computed_hashes[j]) 
          << "Hashes should be different for different content";
    }
  }
}

// Test that identical content produces identical hashes regardless of file size - with mocked Ollama
TEST_F(FileProcessingServiceTest, ProcessFile_HashConsistencyAcrossSizes) {
  // Arrange - Use real ContentExtractor but mock OllamaClient
  auto real_content_extractor = std::make_shared<magic_core::ContentExtractor>();
  auto mock_ollama_client = std::make_shared<magic_tests::MockOllamaClient>();
  
  auto real_file_processing_service = std::make_unique<FileProcessingService>(
      metadata_store_, real_content_extractor, mock_ollama_client);

  // Test the same content in different file sizes
  std::string test_content = "This is test content for consistency verification.";
  std::vector<std::string> file_names = {
    "consistency_test_1.txt",
    "consistency_test_2.txt", 
    "consistency_test_3.txt",
    "consistency_test_4.txt"
  };

  std::vector<std::string> computed_hashes;
  
  for (size_t i = 0; i < file_names.size(); ++i) {
    auto file_path = std::filesystem::temp_directory_path() / file_names[i];
    
    // Create file with identical content
    std::ofstream file(file_path);
    file << test_content;
    file.close();

    std::cout << "=== CONSISTENCY TEST " << i << " ===" << std::endl;
    std::cout << "File: " << file_names[i] << std::endl;
    std::cout << "File size: " << std::filesystem::file_size(file_path) << std::endl;

    // Set up mock expectation for embedding
    auto embedding = create_test_embedding();
    EXPECT_CALL(*mock_ollama_client, get_embedding(test_content))
        .WillOnce(testing::Return(embedding));

    // Process file
    real_file_processing_service->process_file(file_path);

    // Get metadata
    auto metadata = metadata_store_->get_file_metadata(file_path);
    ASSERT_TRUE(metadata.has_value());
    
    std::cout << "Computed hash: " << metadata->content_hash << std::endl;
    
    // Verify hash is valid
    EXPECT_FALSE(metadata->content_hash.empty());
    EXPECT_EQ(metadata->content_hash.length(), 64);
    
    std::regex hex_regex("^[0-9a-f]{64}$");
    EXPECT_TRUE(std::regex_match(metadata->content_hash, hex_regex));
    
    // Store hash for comparison
    computed_hashes.push_back(metadata->content_hash);
    
    // Clean up
    std::filesystem::remove(file_path);
  }

  // Verify all hashes are identical (same content)
  for (size_t i = 1; i < computed_hashes.size(); ++i) {
    EXPECT_EQ(computed_hashes[0], computed_hashes[i]) 
        << "Hashes should be identical for identical content";
  }
}

}  // namespace magic_services