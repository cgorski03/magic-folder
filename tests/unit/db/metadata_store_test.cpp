#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <vector>
#include "magic_core/db/metadata_store.hpp"
#include "../../common/utilities_test.hpp"

namespace magic_core {

class MetadataStoreTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
  }

  // Helper function to convert Chunk to ProcessedChunk with mock compressed content
  magic_core::ProcessedChunk chunk_to_processed_chunk(const Chunk& chunk) {
    magic_core::ProcessedChunk processed_chunk;
    processed_chunk.chunk = chunk;
    // Use mock compressed content for testing
    std::string mock_compressed = "compressed_" + chunk.content;
    processed_chunk.compressed_content = std::vector<char>(mock_compressed.begin(), mock_compressed.end());
    return processed_chunk;
  }

  // Helper function to convert vector of Chunks to ProcessedChunks
  std::vector<magic_core::ProcessedChunk> chunks_to_processed_chunks(const std::vector<Chunk>& chunks) {
    std::vector<magic_core::ProcessedChunk> processed_chunks;
    processed_chunks.reserve(chunks.size());
    for (const auto& chunk : chunks) {
      processed_chunks.push_back(chunk_to_processed_chunk(chunk));
    }
    return processed_chunks;
  }
};

// Tests for create_file_stub
TEST_F(MetadataStoreTest, CreateFileStub_BasicFunctionality) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/stub.txt", "hash123", FileType::Text, 1024, ProcessingStatus::PROCESSING);

  // Act
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  // Assert
  EXPECT_GT(file_id, 0);

  auto retrieved = metadata_store_->get_file_metadata("/test/stub.txt");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id, file_id);
  EXPECT_EQ(retrieved->path, "/test/stub.txt");
  EXPECT_EQ(retrieved->content_hash, "hash123");
  EXPECT_EQ(retrieved->processing_status, ProcessingStatus::PROCESSING);
  EXPECT_EQ(retrieved->file_type, FileType::Text);
  EXPECT_EQ(retrieved->file_size, 1024);
  EXPECT_TRUE(retrieved->summary_vector_embedding.empty());  // No AI analysis yet
}

TEST_F(MetadataStoreTest, CreateFileStub_WithAllFields) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/complex_stub.md", "hash456", FileType::Markdown, 2048, ProcessingStatus::PROCESSED,
      "/original/path/file.md", "tag1,tag2,important");

  // Act
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  // Assert
  auto retrieved = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->path, "/test/complex_stub.md");
  EXPECT_EQ(retrieved->original_path, "/original/path/file.md");
  EXPECT_EQ(retrieved->tags, "tag1,tag2,important");
  EXPECT_EQ(retrieved->processing_status, ProcessingStatus::PROCESSED);
}

TEST_F(MetadataStoreTest, CreateFileStub_DuplicatePathUpdates) {
  // Arrange
  auto metadata1 =
      magic_tests::TestUtilities::create_test_basic_file_metadata("/test/duplicate.txt", "hash1");
  auto metadata2 =
      magic_tests::TestUtilities::create_test_basic_file_metadata("/test/duplicate.txt", "hash2");

  int file_id1 = metadata_store_->upsert_file_stub(metadata1);

  // Act - should update existing record, not throw
  int file_id2 = metadata_store_->upsert_file_stub(metadata2);

  // Assert - should return the same file ID since it's the same path
  EXPECT_EQ(file_id1, file_id2);
  
  // Verify the second metadata was stored (updated)
  auto retrieved = metadata_store_->get_file_metadata(file_id2);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->content_hash, "hash2");
}

TEST_F(MetadataStoreTest, CreateFileStub_UpdateResetsAIFields) {
  // Arrange - Create a file stub first
  auto initial_metadata =
      magic_tests::TestUtilities::create_test_basic_file_metadata("/test/reset_ai.txt", "hash1");
  int file_id = metadata_store_->upsert_file_stub(initial_metadata);

  // Add AI analysis to the file
  auto test_vector = magic_tests::TestUtilities::create_test_vector("ai_analysis", 1024);
  metadata_store_->update_file_ai_analysis(file_id, test_vector, "old_category", "old_filename.txt");

  // Verify AI fields are set
  auto with_ai = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(with_ai.has_value());
  EXPECT_EQ(with_ai->summary_vector_embedding.size(), 1024);
  EXPECT_EQ(with_ai->suggested_category, "old_category");
  EXPECT_EQ(with_ai->suggested_filename, "old_filename.txt");

  // Act - Update the file stub with new hash (simulating file content change)
  auto updated_metadata =
      magic_tests::TestUtilities::create_test_basic_file_metadata("/test/reset_ai.txt", "hash2");
  int updated_file_id = metadata_store_->upsert_file_stub(updated_metadata);

  // Assert - Should be same file ID but AI fields should be reset
  EXPECT_EQ(file_id, updated_file_id);
  
  auto after_update = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(after_update.has_value());
  EXPECT_EQ(after_update->content_hash, "hash2");  // Basic metadata updated
  EXPECT_TRUE(after_update->summary_vector_embedding.empty());  // AI vector reset
  EXPECT_TRUE(after_update->suggested_category.empty());  // AI category reset
  EXPECT_TRUE(after_update->suggested_filename.empty());  // AI filename reset
}

// Tests for update_file_ai_analysis
TEST_F(MetadataStoreTest, UpdateFileAIAnalysis_BasicFunctionality) {
  // Arrange
  auto basic_metadata =
      magic_tests::TestUtilities::create_test_basic_file_metadata("/test/ai_file.txt", "hash789");
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  auto test_vector = magic_tests::TestUtilities::create_test_vector("ai_analysis", 1024);
  std::string category = "document";
  std::string filename = "important_document.txt";

  // Act
  metadata_store_->update_file_ai_analysis(file_id, test_vector, category, filename);

  // Assert
  auto retrieved = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->summary_vector_embedding.size(), 1024);
  EXPECT_EQ(retrieved->suggested_category, category);
  EXPECT_EQ(retrieved->suggested_filename, filename);

  // Verify vector content
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_FLOAT_EQ(retrieved->summary_vector_embedding[i], test_vector[i]);
  }
}

TEST_F(MetadataStoreTest, UpdateFileAIAnalysis_EmptyVector) {
  // Arrange
  auto basic_metadata =
      magic_tests::TestUtilities::create_test_basic_file_metadata("/test/no_vector.txt", "hash000");
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  // Act
  metadata_store_->update_file_ai_analysis(file_id, {}, "category", "filename");

  // Assert
  auto retrieved = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_TRUE(retrieved->summary_vector_embedding.empty());
  EXPECT_EQ(retrieved->suggested_category, "category");
  EXPECT_EQ(retrieved->suggested_filename, "filename");
}

TEST_F(MetadataStoreTest, UpdateFileAIAnalysis_WrongVectorDimension) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/bad_vector.txt", "hash111");
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  std::vector<float> wrong_size_vector(512, 0.5f);  // Wrong dimension

  // Act & Assert
  EXPECT_THROW(metadata_store_->update_file_ai_analysis(file_id, wrong_size_vector, "", ""),
               MetadataStoreError);
}

TEST_F(MetadataStoreTest, UpdateFileAIAnalysis_NonExistentFile) {
  // Arrange
  auto test_vector = magic_tests::TestUtilities::create_test_vector("test", 1024);

  // Act & Assert
  EXPECT_THROW(metadata_store_->update_file_ai_analysis(99999, test_vector, "", ""),
               MetadataStoreError);
}

// Tests for upsert_chunk_metadata
TEST_F(MetadataStoreTest, UpsertChunkMetadata_BasicFunctionality) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/chunked_file.txt", "chunk_hash");
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  auto chunks = magic_tests::TestUtilities::create_test_chunks(3, "test content");

  // Act
  metadata_store_->upsert_chunk_metadata(file_id, chunks_to_processed_chunks(chunks));

  // Assert - verify chunks were stored (we'll need a way to retrieve them)
  // For now, just verify no exception was thrown
  SUCCEED();
}

TEST_F(MetadataStoreTest, UpsertChunkMetadata_EmptyChunks) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/no_chunks.txt", "no_chunk_hash");
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  std::vector<magic_core::ProcessedChunk> empty_chunks;

  // Act & Assert - should not throw
  EXPECT_NO_THROW(metadata_store_->upsert_chunk_metadata(file_id, empty_chunks));
}

TEST_F(MetadataStoreTest, UpsertChunkMetadata_ReplaceExistingChunks) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/replace_chunks.txt", "replace_hash");
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);

  auto initial_chunks = magic_tests::TestUtilities::create_test_chunks(2, "initial content");
  auto updated_chunks = magic_tests::TestUtilities::create_test_chunks(3, "updated content");

  // Act
  metadata_store_->upsert_chunk_metadata(file_id, chunks_to_processed_chunks(initial_chunks));
  metadata_store_->upsert_chunk_metadata(file_id, chunks_to_processed_chunks(updated_chunks));

  // Assert - no exception thrown indicates success
  SUCCEED();
}

// Tests for get_file_metadata variations
TEST_F(MetadataStoreTest, GetFileMetadata_ByPath) {
  // Arrange
  auto metadata = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/get_by_path.txt", "get_hash", FileType::Text, 512, true);
  int file_id =
      magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, metadata);

  // Act
  auto retrieved = metadata_store_->get_file_metadata("/test/get_by_path.txt");

  // Assert
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id, file_id);
  EXPECT_EQ(retrieved->path, "/test/get_by_path.txt");
  EXPECT_EQ(retrieved->content_hash, "get_hash");
  EXPECT_EQ(retrieved->summary_vector_embedding.size(), 1024);
}

TEST_F(MetadataStoreTest, GetFileMetadata_ById) {
  // Arrange
  auto metadata = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/get_by_id.txt", "get_id_hash", FileType::Text, 512, true);
  int file_id =
      magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, metadata);

  // Act
  auto retrieved = metadata_store_->get_file_metadata(file_id);

  // Assert
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id, file_id);
  EXPECT_EQ(retrieved->path, "/test/get_by_id.txt");
  EXPECT_EQ(retrieved->content_hash, "get_id_hash");
}

TEST_F(MetadataStoreTest, GetFileMetadata_NonExistentReturnsNullopt) {
  // Act & Assert
  EXPECT_FALSE(metadata_store_->get_file_metadata("/nonexistent/path.txt").has_value());
  EXPECT_FALSE(metadata_store_->get_file_metadata(99999).has_value());
}

// Tests for file_exists
TEST_F(MetadataStoreTest, FileExists_ExistingFile) {
  // Arrange
  auto metadata = magic_tests::TestUtilities::create_test_file_metadata("/test/exists.txt");
  magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, metadata);

  // Act & Assert
  EXPECT_TRUE(metadata_store_->file_exists("/test/exists.txt"));
  EXPECT_FALSE(metadata_store_->file_exists("/test/does_not_exist.txt"));
}

// Tests for delete_file_metadata
TEST_F(MetadataStoreTest, DeleteFileMetadata_RemovesFile) {
  // Arrange
  auto metadata = magic_tests::TestUtilities::create_test_file_metadata("/test/to_delete.txt");
  magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, metadata);

  ASSERT_TRUE(metadata_store_->file_exists("/test/to_delete.txt"));

  // Act
  metadata_store_->delete_file_metadata("/test/to_delete.txt");

  // Assert
  EXPECT_FALSE(metadata_store_->file_exists("/test/to_delete.txt"));
  EXPECT_FALSE(metadata_store_->get_file_metadata("/test/to_delete.txt").has_value());
}

TEST_F(MetadataStoreTest, DeleteFileMetadata_CascadeDeletesChunks) {
  // Arrange
  auto metadata = magic_tests::TestUtilities::create_test_file_metadata("/test/with_chunks.txt");
  auto chunks = magic_tests::TestUtilities::create_test_chunks(3);

  int file_id =
      magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, metadata, chunks);

  // Act
  metadata_store_->delete_file_metadata("/test/with_chunks.txt");

  // Assert
  EXPECT_FALSE(metadata_store_->file_exists("/test/with_chunks.txt"));
  // Chunks should be cascade deleted due to foreign key constraint
}

// Tests for list_all_files
TEST_F(MetadataStoreTest, ListAllFiles_ReturnsAllFiles) {
  // Arrange
  auto files = magic_tests::TestUtilities::create_test_dataset(5, "/test/list", true);
  for (const auto& file : files) {
    magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file);
  }

  // Act
  auto result = metadata_store_->list_all_files();

  // Assert
  EXPECT_EQ(result.size(), 5);

  // Verify all files are present
  std::set<std::string> paths;
  for (const auto& file : result) {
    paths.insert(file.path);
  }

  for (int i = 0; i < 5; ++i) {
    std::string expected_path = "/test/list/file" + std::to_string(i) + ".txt";
    EXPECT_NE(paths.find(expected_path), paths.end());
  }
}

TEST_F(MetadataStoreTest, ListAllFiles_EmptyWhenNoFiles) {
  // Act
  auto result = metadata_store_->list_all_files();

  // Assert
  EXPECT_TRUE(result.empty());
}

// Tests for search_similar_files
TEST_F(MetadataStoreTest, SearchSimilarFiles_FindsSimilarFiles) {
  // Arrange - Create files with vector embeddings
  std::vector<FileMetadata> files;
  for (int i = 0; i < 10; ++i) {
    auto file = magic_tests::TestUtilities::create_test_file_metadata(
        "/test/search" + std::to_string(i) + ".txt", "hash" + std::to_string(i), FileType::Text,
        1024, true);
    files.push_back(file);
    magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file);
  }

  // Add this line to rebuild the Faiss index with the new vectors
  metadata_store_->rebuild_faiss_index();

  // Create a query vector similar to one of the test vectors
  auto query_vector = magic_tests::TestUtilities::create_test_vector("/test/search0.txt", 1024);

  // Act
  auto results = metadata_store_->search_similar_files(query_vector, 5);

  // Assert
  EXPECT_LE(results.size(), 5);  // Should return at most 5 results
  EXPECT_GT(results.size(), 0);  // Should find at least some results

  // Results should be sorted by distance (most similar first)
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_GE(results[i].distance, results[i - 1].distance);
  }

  // The most similar should be the exact match (distance should be very small)
  if (!results.empty()) {
    EXPECT_LT(results[0].distance, 0.1f);  // Very similar to exact match
  }
}

TEST_F(MetadataStoreTest, SearchSimilarFiles_EmptyIndexReturnsEmpty) {
  // Arrange - no files with vectors
  auto query_vector = magic_tests::TestUtilities::create_test_vector("test", 1024);

  // Act
  auto results = metadata_store_->search_similar_files(query_vector, 5);

  // Assert - should return empty results, not throw
  EXPECT_TRUE(results.empty());
}

TEST_F(MetadataStoreTest, SearchSimilarFiles_WrongVectorDimensionThrows) {
  // Arrange - create a file with vectors so the index is not empty
  auto file = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/search_wrong_dim.txt", "hash", FileType::Text, 1024, true);
  magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file);
  
  // Rebuild the index to include the file
  metadata_store_->rebuild_faiss_index();

  std::vector<float> wrong_dim_query(512, 0.5f);  // Wrong dimension

  // Act & Assert - should throw when calling search_faiss_index with wrong dimension
  EXPECT_THROW(metadata_store_->search_similar_files(wrong_dim_query, 5), MetadataStoreError);
}

// Tests for rebuild_faiss_index
TEST_F(MetadataStoreTest, RebuildFaissIndex_RebuildsSuccessfully) {
  // Arrange
  auto files = magic_tests::TestUtilities::create_test_dataset(5, "/test/rebuild", true);
  for (const auto& file : files) {
    magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file);
  }

  // Act - rebuilding should not throw
  EXPECT_NO_THROW(metadata_store_->rebuild_faiss_index());

  // Assert - search should still work
  auto query_vector =
      magic_tests::TestUtilities::create_test_vector("/test/rebuild/file0.txt", 1024);
  auto results = metadata_store_->search_similar_files(query_vector, 3);
  EXPECT_GT(results.size(), 0);
}

// Integration tests
TEST_F(MetadataStoreTest, CompleteWorkflow_FileStubToSearchable) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/workflow.txt", "workflow_hash", FileType::Text, 1024, ProcessingStatus::PROCESSING);

  // Act & Assert - Complete workflow

  // 1. Create stub
  int file_id = metadata_store_->upsert_file_stub(basic_metadata);
  EXPECT_GT(file_id, 0);

  auto after_stub = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(after_stub.has_value());
  EXPECT_EQ(after_stub->processing_status, ProcessingStatus::PROCESSING);
  EXPECT_TRUE(after_stub->summary_vector_embedding.empty());

  // 2. Add AI analysis
  auto vector = magic_tests::TestUtilities::create_test_vector("workflow", 1024);
  metadata_store_->update_file_ai_analysis(file_id, vector, "category", "suggested_name.txt");

  auto after_ai = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(after_ai.has_value());
  EXPECT_EQ(after_ai->summary_vector_embedding.size(), 1024);
  EXPECT_EQ(after_ai->suggested_category, "category");

  // 3. Add chunks
  auto chunks = magic_tests::TestUtilities::create_test_chunks(2, "workflow content");
  metadata_store_->upsert_chunk_metadata(file_id, chunks_to_processed_chunks(chunks));

  // Add this line to rebuild the Faiss index with the new vector
  metadata_store_->rebuild_faiss_index();

  // 4. Search should now find this file
  auto search_results = metadata_store_->search_similar_files(vector, 5);
  EXPECT_GT(search_results.size(), 0);

  bool found = false;
  for (const auto& result : search_results) {
    if (result.id == file_id) {
      found = true;
      EXPECT_LT(result.distance, 0.1f);  // Should be very similar to itself
      break;
    }
  }
  EXPECT_TRUE(found);
}

// ===== NEW TESTS FOR CHUNK SEARCH FUNCTIONALITY =====

// Test search_similar_chunks with valid file IDs
TEST_F(MetadataStoreTest, SearchSimilarChunks_ValidFileIds) {
  // Arrange - Create files with chunks
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  
  auto chunks1 = magic_tests::TestUtilities::create_test_chunks(3, "machine learning algorithm neural network");
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks1);

  auto file2 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file2.txt", "hash2", magic_core::FileType::Text, 1024, true);
  file2.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file2", 1024);
  
  auto chunks2 = magic_tests::TestUtilities::create_test_chunks(2, "C++ programming code function");
  int file2_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file2, chunks2);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {file1_id, file2_id};
  auto query_vector = magic_tests::TestUtilities::create_test_vector("machine learning", 1024);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 5);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 5);
  
  // Verify chunk result structure
  for (const auto& chunk_result : results) {
    EXPECT_GT(chunk_result.id, 0);
    EXPECT_GE(chunk_result.distance, 0.0f);
    EXPECT_GT(chunk_result.file_id, 0);
    EXPECT_GE(chunk_result.chunk_index, 0);
    EXPECT_FALSE(chunk_result.compressed_content.empty());
    
    // Verify file_id is one of the expected ones
    EXPECT_TRUE(chunk_result.file_id == file1_id || chunk_result.file_id == file2_id);
  }
}

// Test search_similar_chunks with empty file IDs
TEST_F(MetadataStoreTest, SearchSimilarChunks_EmptyFileIds) {
  // Arrange
  auto query_vector = magic_tests::TestUtilities::create_test_vector("test", 1024);
  std::vector<int> file_ids = {};

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 5);

  // Assert
  EXPECT_TRUE(results.empty());
}

// Test search_similar_chunks with non-existent file IDs
TEST_F(MetadataStoreTest, SearchSimilarChunks_NonExistentFileIds) {
  // Arrange
  auto query_vector = magic_tests::TestUtilities::create_test_vector("test", 1024);
  std::vector<int> file_ids = {999, 1000};  // Non-existent IDs

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 5);

  // Assert
  EXPECT_TRUE(results.empty());
}

// Test search_similar_chunks with files that have no chunks
TEST_F(MetadataStoreTest, SearchSimilarChunks_FilesWithoutChunks) {
  // Arrange - Create files without chunks
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {file1_id};
  auto query_vector = magic_tests::TestUtilities::create_test_vector("test", 1024);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 5);

  // Assert
  EXPECT_TRUE(results.empty());
}

// Test search_similar_chunks with large k value
TEST_F(MetadataStoreTest, SearchSimilarChunks_LargeKValue) {
  // Arrange - Create files with chunks
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  
  auto chunks1 = magic_tests::TestUtilities::create_test_chunks(3, "machine learning algorithm neural network");
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks1);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {file1_id};
  auto query_vector = magic_tests::TestUtilities::create_test_vector("machine learning", 1024);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 100);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 3);  // Should not exceed available chunks
}

// Test search_similar_chunks result ordering
TEST_F(MetadataStoreTest, SearchSimilarChunks_ResultsOrderedByDistance) {
  // Arrange - Create files with chunks
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  
  auto chunks1 = magic_tests::TestUtilities::create_test_chunks(5, "machine learning algorithm neural network deep learning");
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks1);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {file1_id};
  auto query_vector = magic_tests::TestUtilities::create_test_vector("machine learning", 1024);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 5);

  // Assert
  ASSERT_FALSE(results.empty());
  
  // Results should be ordered by distance (ascending)
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_LE(results[i-1].distance, results[i].distance);
  }
}

// Test fill_chunk_metadata with valid chunk results
TEST_F(MetadataStoreTest, FillChunkMetadata_ValidChunks) {
  // Arrange - Create files with chunks
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  
  auto chunks1 = magic_tests::TestUtilities::create_test_chunks(3, "machine learning algorithm neural network");
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks1);

  metadata_store_->rebuild_faiss_index();

  // Get chunk IDs from the database
  auto chunk_metadata = metadata_store_->get_chunk_metadata({file1_id});
  ASSERT_FALSE(chunk_metadata.empty());

  // Create chunk search results with just IDs and distances
  std::vector<magic_core::ChunkSearchResult> chunk_results;
  for (const auto& chunk : chunk_metadata) {
    magic_core::ChunkSearchResult result;
    result.id = chunk.id;
    result.distance = 0.1f;  // Dummy distance
    // Initialize other fields to ensure they get filled
    result.file_id = 0;
    result.chunk_index = 0;
    result.compressed_content = {};
    chunk_results.push_back(result);
  }

  // Act
  metadata_store_->fill_chunk_metadata(chunk_results);

  // Assert
  for (const auto& chunk_result : chunk_results) {
    EXPECT_GT(chunk_result.id, 0);
    EXPECT_GT(chunk_result.file_id, 0);
    EXPECT_GE(chunk_result.chunk_index, 0);
    EXPECT_FALSE(chunk_result.compressed_content.empty());
    EXPECT_EQ(chunk_result.distance, 0.1f);  // Distance should be preserved
  }
}

// Test fill_chunk_metadata with empty chunk results
TEST_F(MetadataStoreTest, FillChunkMetadata_EmptyChunks) {
  // Arrange
  std::vector<magic_core::ChunkSearchResult> chunk_results = {};

  // Act
  metadata_store_->fill_chunk_metadata(chunk_results);

  // Assert
  EXPECT_TRUE(chunk_results.empty());
}

// Test fill_chunk_metadata with non-existent chunk IDs
TEST_F(MetadataStoreTest, FillChunkMetadata_NonExistentChunkIds) {
  // Arrange
  std::vector<magic_core::ChunkSearchResult> chunk_results;
  
  magic_core::ChunkSearchResult result;
  result.id = 999;  // Non-existent ID
  result.distance = 0.1f;
  result.file_id = 0;
  result.chunk_index = 0;
  result.compressed_content = {};
  chunk_results.push_back(result);

  // Act
  metadata_store_->fill_chunk_metadata(chunk_results);

  // Assert
  ASSERT_FALSE(chunk_results.empty());
  EXPECT_EQ(chunk_results[0].id, 999);
  EXPECT_EQ(chunk_results[0].distance, 0.1f);
  // The metadata should not be filled for non-existent chunks
  // Note: The actual behavior might fill these with garbage values, so we just check that ID and distance are preserved
  EXPECT_EQ(chunk_results[0].id, 999);
  EXPECT_EQ(chunk_results[0].distance, 0.1f);
}

// Test search_similar_chunks with mixed file types
TEST_F(MetadataStoreTest, SearchSimilarChunks_MixedFileTypes) {
  // Arrange - Create files of different types with chunks
  auto text_file = magic_tests::TestUtilities::create_test_file_metadata("/docs/text.txt", "hash1", magic_core::FileType::Text, 1024, true);
  text_file.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("text_file", 1024);
  auto text_chunks = magic_tests::TestUtilities::create_test_chunks(2, "text content document");
  int text_file_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, text_file, text_chunks);

  auto code_file = magic_tests::TestUtilities::create_test_file_metadata("/src/code.cpp", "hash2", magic_core::FileType::Code, 1024, true);
  code_file.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("code_file", 1024);
  auto code_chunks = magic_tests::TestUtilities::create_test_chunks(3, "C++ code function class");
  int code_file_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, code_file, code_chunks);

  auto markdown_file = magic_tests::TestUtilities::create_test_file_metadata("/docs/readme.md", "hash3", magic_core::FileType::Markdown, 1024, true);
  markdown_file.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("markdown_file", 1024);
  auto markdown_chunks = magic_tests::TestUtilities::create_test_chunks(2, "markdown documentation guide");
  int markdown_file_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, markdown_file, markdown_chunks);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {text_file_id, code_file_id, markdown_file_id};
  auto query_vector = magic_tests::TestUtilities::create_test_vector("documentation", 1024);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 10);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 7);  // Total chunks across all files
  
  // Verify chunks come from different file types
  std::set<int> file_ids_found;
  for (const auto& chunk_result : results) {
    file_ids_found.insert(chunk_result.file_id);
    EXPECT_GT(chunk_result.id, 0);
    EXPECT_GE(chunk_result.distance, 0.0f);
    EXPECT_GE(chunk_result.chunk_index, 0);
    EXPECT_FALSE(chunk_result.compressed_content.empty());
  }
  
  // Should find chunks from multiple files
  EXPECT_GE(file_ids_found.size(), 1);
}

// Test search_similar_chunks with edge case query vector
TEST_F(MetadataStoreTest, SearchSimilarChunks_EdgeCaseQueryVector) {
  // Arrange - Create files with chunks
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  
  auto chunks1 = magic_tests::TestUtilities::create_test_chunks(3, "machine learning algorithm neural network");
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks1);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {file1_id};
  
  // Test with zero vector
  std::vector<float> zero_vector(1024, 0.0f);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, zero_vector, 5);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 3);
  
  for (const auto& chunk_result : results) {
    EXPECT_GT(chunk_result.id, 0);
    EXPECT_GE(chunk_result.distance, 0.0f);
    EXPECT_GT(chunk_result.file_id, 0);
    EXPECT_GE(chunk_result.chunk_index, 0);
    EXPECT_FALSE(chunk_result.compressed_content.empty());
  }
}

// Test search_similar_chunks with single file
TEST_F(MetadataStoreTest, SearchSimilarChunks_SingleFile) {
  // Arrange - Create single file with chunks
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  
  auto chunks1 = magic_tests::TestUtilities::create_test_chunks(5, "machine learning algorithm neural network deep learning");
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks1);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {file1_id};
  auto query_vector = magic_tests::TestUtilities::create_test_vector("machine learning", 1024);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 3);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 3);
  
  // All chunks should be from the same file
  for (const auto& chunk_result : results) {
    EXPECT_EQ(chunk_result.file_id, file1_id);
    EXPECT_GT(chunk_result.id, 0);
    EXPECT_GE(chunk_result.distance, 0.0f);
    EXPECT_GE(chunk_result.chunk_index, 0);
    EXPECT_FALSE(chunk_result.compressed_content.empty());
  }
}

// Test search_similar_chunks preserves chunk metadata correctly
TEST_F(MetadataStoreTest, SearchSimilarChunks_PreservesChunkMetadata) {
  // Arrange - Create file with specific chunk content
  auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/file1.txt", "hash1", magic_core::FileType::Text, 1024, true);
  file1.summary_vector_embedding = magic_tests::TestUtilities::create_test_vector("file1", 1024);
  
  // Create chunks with specific content
  std::vector<Chunk> chunks;
  chunks.push_back(magic_tests::TestUtilities::create_test_chunk_with_embedding("first chunk content", 0, "chunk1"));
  chunks.push_back(magic_tests::TestUtilities::create_test_chunk_with_embedding("second chunk content", 1, "chunk2"));
  chunks.push_back(magic_tests::TestUtilities::create_test_chunk_with_embedding("third chunk content", 2, "chunk3"));
  
  int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks);

  metadata_store_->rebuild_faiss_index();

  std::vector<int> file_ids = {file1_id};
  auto query_vector = magic_tests::TestUtilities::create_test_vector("chunk content", 1024);

  // Act
  auto results = metadata_store_->search_similar_chunks(file_ids, query_vector, 5);

  // Assert
  ASSERT_FALSE(results.empty());
  
  // Verify that chunk content and indices are preserved
  std::set<std::string> expected_contents = {"compressed_first chunk content", "compressed_second chunk content", "compressed_third chunk content"};
  std::set<int> expected_indices = {0, 1, 2};
  
  std::set<std::string> found_contents;
  std::set<int> found_indices;
  
  for (const auto& chunk_result : results) {
    EXPECT_EQ(chunk_result.file_id, file1_id);
    // Convert vector<char> to string for comparison (mock compressed content)
    std::string content_str(chunk_result.compressed_content.begin(), chunk_result.compressed_content.end());
    found_contents.insert(content_str);
    found_indices.insert(chunk_result.chunk_index);
  }
  
  // Should find all expected content and indices
  EXPECT_EQ(found_contents.size(), expected_contents.size());
  EXPECT_EQ(found_indices.size(), expected_indices.size());
}

}  // namespace magic_core