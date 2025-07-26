#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "magic_core/services/file_processing_service.hpp"
#include "test_mocks.hpp"
#include "test_utilities.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace magic_core {

class FileProcessingServiceTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    
    // Create mock dependencies
    mock_content_extractor_factory_ = std::make_shared<StrictMock<magic_tests::MockContentExtractorFactory>>();
    mock_content_extractor_ = std::make_unique<StrictMock<magic_tests::MockContentExtractor>>();
    mock_ollama_client_ = std::make_shared<StrictMock<magic_tests::MockOllamaClient>>();
    
    // Create the service
    file_processing_service_ = std::make_unique<FileProcessingService>(
        metadata_store_, mock_content_extractor_factory_, mock_ollama_client_);
    
    // Setup test file path and create minimal test file
    setupTestFile();
    
    // Setup expected basic metadata
    setup_expected_metadata();
  }

  void TearDown() override {
    // Clean up test file
    if (std::filesystem::exists(test_file_path_)) {
      std::filesystem::remove(test_file_path_);
    }
    MetadataStoreTestBase::TearDown();
  }

  void setupTestFile(const std::string& content = "Test file content for processing") {
    // Create test file in temp directory (following existing patterns)
    test_file_path_ = std::filesystem::temp_directory_path() / "test_document.txt";
    std::ofstream file(test_file_path_);
    file << content;
    file.close();
  }

  void setup_expected_metadata() {
    expected_basic_metadata_.path = test_file_path_;
    expected_basic_metadata_.file_size = std::filesystem::file_size(test_file_path_);
    expected_basic_metadata_.file_hash = "test_hash_123";
    expected_basic_metadata_.processing_status = "IDLE";
    expected_basic_metadata_.file_type = FileType::Text;
    expected_basic_metadata_.tags = "";
    expected_basic_metadata_.original_path = test_file_path_;
  }

  // Helper to create test chunks
  std::vector<Chunk> create_test_chunks(int count = 3) {
    std::vector<Chunk> chunks;
    for (int i = 0; i < count; ++i) {
      chunks.push_back({
        .content = "Test chunk content " + std::to_string(i),
        .chunk_index = i
      });
    }
    return chunks;
  }

  // Helper to create test embeddings
  std::vector<float> create_test_embedding(float base_value = 0.1f) {
    return magic_tests::TestUtilities::create_test_vector("test", MetadataStore::VECTOR_DIMENSION);
  }

  // Helper to setup extraction expectations
  void setup_extraction_expectations(const std::vector<Chunk>& chunks, const std::string& hash) {
    ExtractionResult extraction_result;
    extraction_result.chunks = chunks;
    extraction_result.content_hash = hash;
    
    // Determine file type based on extension (like real extractors)
    if (test_file_path_.extension() == ".md") {
      extraction_result.file_type = FileType::Markdown;
    } else {
      extraction_result.file_type = FileType::Text;
    }
    
    EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(test_file_path_))
        .WillOnce(testing::ReturnRef(*mock_content_extractor_));
    
    EXPECT_CALL(*mock_content_extractor_, extract_with_hash(test_file_path_))
        .WillOnce(Return(extraction_result));
  }

  // Helper to setup embedding expectations
  void setup_embedding_expectations(const std::vector<Chunk>& chunks) {
    for (size_t i = 0; i < chunks.size(); ++i) {
      auto embedding = create_test_embedding(0.1f + i * 0.1f);
      EXPECT_CALL(*mock_ollama_client_, get_embedding(chunks[i].content))
          .WillOnce(Return(embedding));
    }
  }

  std::unique_ptr<FileProcessingService> file_processing_service_;
  std::shared_ptr<StrictMock<magic_tests::MockContentExtractorFactory>> mock_content_extractor_factory_;
  std::unique_ptr<StrictMock<magic_tests::MockContentExtractor>> mock_content_extractor_;
  std::shared_ptr<StrictMock<magic_tests::MockOllamaClient>> mock_ollama_client_;
  
  std::filesystem::path test_file_path_;
  BasicFileMetadata expected_basic_metadata_;
};

// Basic functionality tests
TEST_F(FileProcessingServiceTest, ProcessFile_BasicFunctionality) {
  // Arrange
  auto test_chunks = create_test_chunks(2);
  std::string expected_hash = "test_hash_123";
  
  setup_extraction_expectations(test_chunks, expected_hash);
  setup_embedding_expectations(test_chunks);
  
  // Act
  auto result = file_processing_service_->process_file(test_file_path_);
  
  // Assert
     EXPECT_TRUE(result.success);
   EXPECT_EQ(result.file_path, test_file_path_);
   EXPECT_EQ(result.file_size, expected_basic_metadata_.file_size);
   EXPECT_EQ(result.content_hash, expected_hash);
   EXPECT_EQ(result.file_type, FileType::Text);
  
  // Verify file was created in metadata store
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->file_hash, expected_hash);
  EXPECT_EQ(stored_metadata->processing_status, "PROCESSING");
  
  // Verify document embedding was stored (should be non-empty)
  EXPECT_FALSE(stored_metadata->summary_vector_embedding.empty());
  EXPECT_EQ(stored_metadata->summary_vector_embedding.size(), MetadataStore::VECTOR_DIMENSION);
}

TEST_F(FileProcessingServiceTest, ProcessFile_DocumentEmbeddingCalculation) {
  // Arrange
  auto test_chunks = create_test_chunks(3);
  
  setup_extraction_expectations(test_chunks, "test_hash");
  
  // Setup specific embeddings to test averaging
  std::vector<std::vector<float>> chunk_embeddings = {
    {1.0f, 0.0f, 0.0f, 0.0f},  // Chunk 0
    {0.0f, 1.0f, 0.0f, 0.0f},  // Chunk 1  
    {0.0f, 0.0f, 1.0f, 0.0f}   // Chunk 2
  };
  
  // Pad to full dimension
  for (auto& embedding : chunk_embeddings) {
    embedding.resize(MetadataStore::VECTOR_DIMENSION, 0.0f);
  }
  
  for (size_t i = 0; i < test_chunks.size(); ++i) {
    EXPECT_CALL(*mock_ollama_client_, get_embedding(test_chunks[i].content))
        .WillOnce(Return(chunk_embeddings[i]));
  }
  
  // Act
  auto result = file_processing_service_->process_file(test_file_path_);
  
  // Assert
  EXPECT_TRUE(result.success);
  
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  
  // Check document embedding is normalized average
  auto& doc_embedding = stored_metadata->summary_vector_embedding;
  EXPECT_GT(doc_embedding[0], 0.0f);  // Should have some value from averaging
  EXPECT_GT(doc_embedding[1], 0.0f);
  EXPECT_GT(doc_embedding[2], 0.0f);
  
  // Verify L2 normalization (magnitude should be ~1.0)
  float magnitude = 0.0f;
  for (float val : doc_embedding) {
    magnitude += val * val;
  }
  magnitude = std::sqrt(magnitude);
  EXPECT_NEAR(magnitude, 1.0f, 0.01f);
}

TEST_F(FileProcessingServiceTest, ProcessFile_ChunkBatching) {
  // Arrange - create enough chunks to trigger batching (> 64)
  auto test_chunks = create_test_chunks(70);
  
  setup_extraction_expectations(test_chunks, "test_hash");
  setup_embedding_expectations(test_chunks);
  
  // Act
  auto result = file_processing_service_->process_file(test_file_path_);
  
  // Assert
  EXPECT_TRUE(result.success);
  
  // Verify all chunks were stored despite batching
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  
  // Note: We'd need to add a method to get chunks count to fully verify this
  // For now, just verify the file was processed successfully
}

TEST_F(FileProcessingServiceTest, ProcessFile_EmptyFile) {
  // Arrange
  std::vector<Chunk> empty_chunks;
  
  setup_extraction_expectations(empty_chunks, "empty_hash");
  // No embedding expectations since no chunks
  
  // Act
  auto result = file_processing_service_->process_file(test_file_path_);
  
  // Assert
  EXPECT_TRUE(result.success);
  
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->file_hash, "empty_hash");
  
  // Document embedding should be empty for files with no chunks
  EXPECT_TRUE(stored_metadata->summary_vector_embedding.empty());
}

TEST_F(FileProcessingServiceTest, ProcessFile_SingleChunk) {
  // Arrange
  auto test_chunks = create_test_chunks(1);
  
  setup_extraction_expectations(test_chunks, "single_hash");
  setup_embedding_expectations(test_chunks);
  
  // Act
  auto result = file_processing_service_->process_file(test_file_path_);
  
  // Assert
  EXPECT_TRUE(result.success);
  
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  
  // For single chunk, document embedding should equal the chunk embedding (normalized)
  EXPECT_FALSE(stored_metadata->summary_vector_embedding.empty());
}

// Error handling tests
TEST_F(FileProcessingServiceTest, ProcessFile_ExtractionError) {
  // Arrange
  EXPECT_CALL(*mock_content_extractor_factory_, get_extractor_for(test_file_path_))
      .WillOnce(testing::ReturnRef(*mock_content_extractor_));
  
  EXPECT_CALL(*mock_content_extractor_, extract_with_hash(test_file_path_))
      .WillOnce(testing::Throw(std::runtime_error("Extraction failed")));
  
  // Act & Assert
  EXPECT_THROW(file_processing_service_->process_file(test_file_path_), std::runtime_error);
}

TEST_F(FileProcessingServiceTest, ProcessFile_EmbeddingError) {
  // Arrange
  auto test_chunks = create_test_chunks(2);
  
  setup_extraction_expectations(test_chunks, "test_hash");
  
  // First embedding succeeds, second fails
  EXPECT_CALL(*mock_ollama_client_, get_embedding(test_chunks[0].content))
      .WillOnce(Return(create_test_embedding()));
  
  EXPECT_CALL(*mock_ollama_client_, get_embedding(test_chunks[1].content))
      .WillOnce(testing::Throw(magic_core::OllamaError("Embedding failed")));
  
  // Act & Assert
  EXPECT_THROW(file_processing_service_->process_file(test_file_path_), magic_core::OllamaError);
}

TEST_F(FileProcessingServiceTest, ProcessFile_MetadataStoreError) {
  // Arrange
  auto test_chunks = create_test_chunks(1);
  
  setup_extraction_expectations(test_chunks, "test_hash");
  // Don't setup embedding expectations - the error happens before embeddings are generated
  
  // Create a file that already exists to trigger unique constraint error
  auto existing_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      test_file_path_, "existing_hash");
  metadata_store_->create_file_stub(existing_metadata);
  
  // Act & Assert
  EXPECT_THROW(file_processing_service_->process_file(test_file_path_), MetadataStoreError);
}

 // File type specific tests
 TEST_F(FileProcessingServiceTest, ProcessFile_MarkdownFile) {
   // Arrange - create a .md file
   test_file_path_ = std::filesystem::temp_directory_path() / "test_document.md";
   std::ofstream md_file(test_file_path_);
   md_file << "# Test Markdown\nThis is a test markdown file.";
   md_file.close();
   
   auto test_chunks = create_test_chunks(3);
   
   setup_extraction_expectations(test_chunks, "md_hash");
   setup_embedding_expectations(test_chunks);
   
   // Act
   auto result = file_processing_service_->process_file(test_file_path_);
   
   // Assert
   EXPECT_TRUE(result.success);
   EXPECT_EQ(result.file_type, FileType::Markdown);
   
   auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
   ASSERT_TRUE(stored_metadata.has_value());
   EXPECT_EQ(stored_metadata->file_type, FileType::Markdown);
   
   // Clean up
   std::filesystem::remove(test_file_path_);
 }

// Integration-style tests
TEST_F(FileProcessingServiceTest, ProcessFile_FullWorkflow) {
  // Arrange
  auto test_chunks = create_test_chunks(5);
  std::string expected_hash = "full_workflow_hash";
  
  setup_extraction_expectations(test_chunks, expected_hash);
  setup_embedding_expectations(test_chunks);
  
  // Act
  auto result = file_processing_service_->process_file(test_file_path_);
  
     // Assert - Verify complete workflow
   EXPECT_TRUE(result.success);
   EXPECT_EQ(result.file_path, test_file_path_);
   EXPECT_EQ(result.content_hash, expected_hash);
  
  // Verify file exists in metadata store
  EXPECT_TRUE(metadata_store_->file_exists(test_file_path_));
  
  // Verify file metadata is complete
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_EQ(stored_metadata->path, test_file_path_);
  EXPECT_EQ(stored_metadata->file_hash, expected_hash);
  EXPECT_EQ(stored_metadata->processing_status, "IDLE");
  EXPECT_FALSE(stored_metadata->summary_vector_embedding.empty());
  
  // Verify document can be found in search (basic test)
  if (!stored_metadata->summary_vector_embedding.empty()) {
    metadata_store_->rebuild_faiss_index();
    auto search_results = metadata_store_->search_similar_files(
        stored_metadata->summary_vector_embedding, 1);
    EXPECT_EQ(search_results.size(), 1);
    EXPECT_EQ(search_results[0].id, stored_metadata->id);
  }
}

// Performance considerations
TEST_F(FileProcessingServiceTest, ProcessFile_LargeFile) {
  // Arrange - simulate large file with many chunks
  auto test_chunks = create_test_chunks(100);
  
  setup_extraction_expectations(test_chunks, "large_file_hash");
  setup_embedding_expectations(test_chunks);
  
  // Act
  auto start_time = std::chrono::high_resolution_clock::now();
  auto result = file_processing_service_->process_file(test_file_path_);
  auto end_time = std::chrono::high_resolution_clock::now();
  
  // Assert
  EXPECT_TRUE(result.success);
  
  // Performance check (adjust based on your requirements)
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  EXPECT_LT(duration.count(), 5000);  // Should complete within 5 seconds
  
  // Verify result quality
  auto stored_metadata = metadata_store_->get_file_metadata(test_file_path_);
  ASSERT_TRUE(stored_metadata.has_value());
  EXPECT_FALSE(stored_metadata->summary_vector_embedding.empty());
}

}  // namespace magic_core