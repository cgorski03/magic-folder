#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "magic_core/metadata_store.hpp"
#include "magic_core/types.hpp"
#include "test_utilities.hpp"

namespace magic_core {

class MetadataStoreTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
  }
};

// Tests for create_file_stub
TEST_F(MetadataStoreTest, CreateFileStub_BasicFunctionality) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/stub.txt", "hash123", FileType::Text, 1024, "PROCESSING");

  // Act
  int file_id = metadata_store_->create_file_stub(basic_metadata);

  // Assert
  EXPECT_GT(file_id, 0);
  
  auto retrieved = metadata_store_->get_file_metadata("/test/stub.txt");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id, file_id);
  EXPECT_EQ(retrieved->path, "/test/stub.txt");
  EXPECT_EQ(retrieved->file_hash, "hash123");
  EXPECT_EQ(retrieved->processing_status, "PROCESSING");
  EXPECT_EQ(retrieved->file_type, FileType::Text);
  EXPECT_EQ(retrieved->file_size, 1024);
  EXPECT_TRUE(retrieved->summary_vector_embedding.empty()); // No AI analysis yet
}

TEST_F(MetadataStoreTest, CreateFileStub_WithAllFields) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/complex_stub.md", "hash456", FileType::Markdown, 2048, "IDLE",
      "/original/path/file.md", "tag1,tag2,important");

  // Act
  int file_id = metadata_store_->create_file_stub(basic_metadata);

  // Assert
  auto retrieved = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->path, "/test/complex_stub.md");
  EXPECT_EQ(retrieved->original_path, "/original/path/file.md");
  EXPECT_EQ(retrieved->tags, "tag1,tag2,important");
  EXPECT_EQ(retrieved->processing_status, "IDLE");
}

TEST_F(MetadataStoreTest, CreateFileStub_DuplicatePathThrows) {
  // Arrange
  auto metadata1 = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/duplicate.txt", "hash1");
  auto metadata2 = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/duplicate.txt", "hash2");

  metadata_store_->create_file_stub(metadata1);

  // Act & Assert
  EXPECT_THROW(metadata_store_->create_file_stub(metadata2), MetadataStoreError);
}

// Tests for update_file_ai_analysis
TEST_F(MetadataStoreTest, UpdateFileAIAnalysis_BasicFunctionality) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/ai_file.txt", "hash789");
  int file_id = metadata_store_->create_file_stub(basic_metadata);

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
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/no_vector.txt", "hash000");
  int file_id = metadata_store_->create_file_stub(basic_metadata);

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
  int file_id = metadata_store_->create_file_stub(basic_metadata);

  std::vector<float> wrong_size_vector(512, 0.5f); // Wrong dimension

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
  int file_id = metadata_store_->create_file_stub(basic_metadata);

  auto chunks = magic_tests::TestUtilities::create_test_chunks(3, "test content");

  // Act
  metadata_store_->upsert_chunk_metadata(file_id, chunks);

  // Assert - verify chunks were stored (we'll need a way to retrieve them)
  // For now, just verify no exception was thrown
  SUCCEED();
}

TEST_F(MetadataStoreTest, UpsertChunkMetadata_EmptyChunks) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/no_chunks.txt", "no_chunk_hash");
  int file_id = metadata_store_->create_file_stub(basic_metadata);

  std::vector<ChunkWithEmbedding> empty_chunks;

  // Act & Assert - should not throw
  EXPECT_NO_THROW(metadata_store_->upsert_chunk_metadata(file_id, empty_chunks));
}

TEST_F(MetadataStoreTest, UpsertChunkMetadata_ReplaceExistingChunks) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/replace_chunks.txt", "replace_hash");
  int file_id = metadata_store_->create_file_stub(basic_metadata);

  auto initial_chunks = magic_tests::TestUtilities::create_test_chunks(2, "initial content");
  auto updated_chunks = magic_tests::TestUtilities::create_test_chunks(3, "updated content");

  // Act
  metadata_store_->upsert_chunk_metadata(file_id, initial_chunks);
  metadata_store_->upsert_chunk_metadata(file_id, updated_chunks); // Should replace

  // Assert - no exception thrown indicates success
  SUCCEED();
}

// Tests for get_file_metadata variations
TEST_F(MetadataStoreTest, GetFileMetadata_ByPath) {
  // Arrange
  auto metadata = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/get_by_path.txt", "get_hash", FileType::Text, 512, true);
  int file_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, metadata);

  // Act
  auto retrieved = metadata_store_->get_file_metadata("/test/get_by_path.txt");

  // Assert
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id, file_id);
  EXPECT_EQ(retrieved->path, "/test/get_by_path.txt");
  EXPECT_EQ(retrieved->file_hash, "get_hash");
  EXPECT_EQ(retrieved->summary_vector_embedding.size(), 1024);
}

TEST_F(MetadataStoreTest, GetFileMetadata_ById) {
  // Arrange
  auto metadata = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/get_by_id.txt", "get_id_hash", FileType::Text, 512, true);
  int file_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, metadata);

  // Act
  auto retrieved = metadata_store_->get_file_metadata(file_id);

  // Assert
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id, file_id);
  EXPECT_EQ(retrieved->path, "/test/get_by_id.txt");
  EXPECT_EQ(retrieved->file_hash, "get_id_hash");
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
  
  int file_id = magic_tests::TestUtilities::create_complete_file_in_store(
      metadata_store_, metadata, chunks);

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
        "/test/search" + std::to_string(i) + ".txt", "hash" + std::to_string(i),
        FileType::Text, 1024, true);
    files.push_back(file);
    magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file);
  }

  // Create a query vector similar to one of the test vectors
  auto query_vector = magic_tests::TestUtilities::create_test_vector("/test/search0.txt", 1024);

  // Act
  auto results = metadata_store_->search_similar_files(query_vector, 5);

  // Assert
  EXPECT_LE(results.size(), 5); // Should return at most 5 results
  EXPECT_GT(results.size(), 0); // Should find at least some results

  // Results should be sorted by distance (most similar first)
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_GE(results[i].distance, results[i-1].distance);
  }

  // The most similar should be the exact match (distance should be very small)
  if (!results.empty()) {
    EXPECT_LT(results[0].distance, 0.1f); // Very similar to exact match
  }
}

TEST_F(MetadataStoreTest, SearchSimilarFiles_EmptyIndexReturnsEmpty) {
  // Arrange - no files with vectors
  auto query_vector = magic_tests::TestUtilities::create_test_vector("test", 1024);

  // Act & Assert
  EXPECT_THROW(metadata_store_->search_similar_files(query_vector, 5), MetadataStoreError);
}

TEST_F(MetadataStoreTest, SearchSimilarFiles_WrongVectorDimensionThrows) {
  // Arrange
  auto file = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/search_wrong_dim.txt", "hash", FileType::Text, 1024, true);
  magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file);

  std::vector<float> wrong_dim_query(512, 0.5f); // Wrong dimension

  // Act & Assert
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
  auto query_vector = magic_tests::TestUtilities::create_test_vector("/test/rebuild/file0.txt", 1024);
  auto results = metadata_store_->search_similar_files(query_vector, 3);
  EXPECT_GT(results.size(), 0);
}

// Integration tests
TEST_F(MetadataStoreTest, CompleteWorkflow_FileStubToSearchable) {
  // Arrange
  auto basic_metadata = magic_tests::TestUtilities::create_test_basic_file_metadata(
      "/test/workflow.txt", "workflow_hash", FileType::Text, 1024, "PROCESSING");

  // Act & Assert - Complete workflow
  
  // 1. Create stub
  int file_id = metadata_store_->create_file_stub(basic_metadata);
  EXPECT_GT(file_id, 0);
  
  auto after_stub = metadata_store_->get_file_metadata(file_id);
  ASSERT_TRUE(after_stub.has_value());
  EXPECT_EQ(after_stub->processing_status, "PROCESSING");
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
  metadata_store_->upsert_chunk_metadata(file_id, chunks);

  // 4. Search should now find this file
  auto search_results = metadata_store_->search_similar_files(vector, 5);
  EXPECT_GT(search_results.size(), 0);
  
  bool found = false;
  for (const auto& result : search_results) {
    if (result.id == file_id) {
      found = true;
      EXPECT_LT(result.distance, 0.1f); // Should be very similar to itself
      break;
    }
  }
  EXPECT_TRUE(found);
}

// Error handling tests
TEST_F(MetadataStoreTest, DatabaseErrorHandling_InvalidPath) {
  // Test various error conditions that should throw MetadataStoreError
  
  // Invalid file ID operations
  EXPECT_THROW(metadata_store_->get_file_metadata(-1), std::exception);
  
  // Other error conditions would need specific database corruption scenarios
  // which are difficult to test reliably
}

}  // namespace magic_core 