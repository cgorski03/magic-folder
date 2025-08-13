#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "magic_core/db/metadata_store.hpp"
#include "magic_core/llm/ollama_client.hpp"
#include "magic_core/services/search_service.hpp"
#include "../../common/mocks_test.hpp"
#include "../../common/utilities_test.hpp"

namespace magic_core {

class SearchServiceTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    // Call parent setup to initialize metadata_store_
    MetadataStoreTestBase::SetUp();

    // Check if Ollama server is running before setting up tests
    try {
      // Try to create the mock client - this will fail if server isn't running
      mock_ollama_client_ = std::make_shared<magic_tests::MockOllamaClient>();
    } catch (const magic_core::OllamaError& e) {
      // Server not running - skip all tests in this fixture
      GTEST_SKIP() << "Ollama server not available: " << e.what();
      return;
    }

    // Create the service with mocked dependencies
    // Provide a no-op decompressor so tests don't depend on zstd
    auto noop_decompress = [](const std::vector<char>& data) {
      return std::string(data.begin(), data.end());
    };
    search_service_ = std::make_unique<magic_core::SearchService>(metadata_store_, mock_ollama_client_, noop_decompress);

    // Metadata store is initialized by the fixture (constructor handles setup with encryption)

    // Set up test data
    setupTestData();
  }

  void TearDown() override {
    MetadataStoreTestBase::TearDown();
  }

  // Helper method to create a test embedding
  std::vector<float> create_test_embedding(float value = 0.1f) {
    return magic_tests::MockUtilities::create_test_embedding(value);
  }

  // Helper method to create a test embedding with custom values
  std::vector<float> create_test_embedding_with_values(const std::vector<float>& values) {
    return magic_tests::MockUtilities::create_test_embedding_with_values(values);
  }

  // Set up test data in the metadata store
  void setupTestData() {
    // Create test files with different embeddings
    std::vector<magic_core::FileMetadata> test_files;

    // File 1: Machine learning related
    auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/ml_algorithms.txt", "hash1", magic_core::FileType::Text, 1024, true);
    file1.summary_vector_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
    test_files.push_back(file1);

    // File 2: Programming related
    auto file2 = magic_tests::TestUtilities::create_test_file_metadata("/src/main.cpp", "hash2", magic_core::FileType::Code, 1024, true);
    file2.summary_vector_embedding = create_test_embedding_with_values({0.1f, 0.2f, 0.3f, 0.4f});
    test_files.push_back(file2);

    // File 3: Documentation related
    auto file3 = magic_tests::TestUtilities::create_test_file_metadata("/docs/README.md", "hash3", magic_core::FileType::Markdown, 1024, true);
    file3.summary_vector_embedding = create_test_embedding_with_values({0.5f, 0.5f, 0.5f, 0.5f});
    test_files.push_back(file3);

    // File 4: Another ML file (similar to file 1)
    auto file4 = magic_tests::TestUtilities::create_test_file_metadata("/docs/neural_networks.txt", "hash4", magic_core::FileType::Text, 1024, true);
    file4.summary_vector_embedding = create_test_embedding_with_values({0.85f, 0.75f, 0.65f, 0.55f});
    test_files.push_back(file4);

    // Add files to metadata store using new API
    for (const auto& file : test_files) {
      magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file);
    }

    // Rebuild the Faiss index
    metadata_store_->rebuild_faiss_index();
  }

  // Set up test data with chunks
  void setupTestDataWithChunks() {
    // Create test files with chunks
    std::vector<magic_core::FileMetadata> test_files;

    // File 1: Machine learning related with chunks
    auto file1 = magic_tests::TestUtilities::create_test_file_metadata("/docs/ml_algorithms.txt", "hash1", magic_core::FileType::Text, 1024, true);
    file1.summary_vector_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
    
    // Create chunks for file 1
    auto chunks1 = magic_tests::TestUtilities::create_test_chunks(3, "machine learning algorithm neural network");
    
    int file1_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file1, chunks1);

    // File 2: Programming related with chunks
    auto file2 = magic_tests::TestUtilities::create_test_file_metadata("/src/main.cpp", "hash2", magic_core::FileType::Code, 1024, true);
    file2.summary_vector_embedding = create_test_embedding_with_values({0.1f, 0.2f, 0.3f, 0.4f});
    
    // Create chunks for file 2
    auto chunks2 = magic_tests::TestUtilities::create_test_chunks(2, "C++ programming code function class");
    
    int file2_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file2, chunks2);

    // File 3: Documentation related with chunks
    auto file3 = magic_tests::TestUtilities::create_test_file_metadata("/docs/README.md", "hash3", magic_core::FileType::Markdown, 1024, true);
    file3.summary_vector_embedding = create_test_embedding_with_values({0.5f, 0.5f, 0.5f, 0.5f});
    
    // Create chunks for file 3
    auto chunks3 = magic_tests::TestUtilities::create_test_chunks(4, "documentation guide tutorial help");
    
    int file3_id = magic_tests::TestUtilities::create_complete_file_in_store(metadata_store_, file3, chunks3);

    // Rebuild the Faiss index
    metadata_store_->rebuild_faiss_index();
  }

  // Helper method to set up mock expectations for query embedding
  void setupQueryEmbeddingExpectation(const std::string& query, const std::vector<float>& embedding) {
    EXPECT_CALL(*mock_ollama_client_, get_embedding(query))
        .WillOnce(testing::Return(embedding));
  }

  std::unique_ptr<SearchService> search_service_;
  std::shared_ptr<magic_tests::MockOllamaClient> mock_ollama_client_;
};

// Test successful search with semantic similarity
TEST_F(SearchServiceTest, Search_SuccessfulSemanticSearch) {
  // Arrange
  std::string query = "machine learning algorithms";
  auto query_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 3);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 3);

  // The most similar files should be the ML-related ones
  bool found_ml_file = false;
  bool found_neural_networks_file = false;

  for (const auto& result : results) {
    if (result.file.path.find("ml_algorithms") != std::string::npos) {
      found_ml_file = true;
    }
    if (result.file.path.find("neural_networks") != std::string::npos) {
      found_neural_networks_file = true;
    }
  }

  EXPECT_TRUE(found_ml_file || found_neural_networks_file);
}

// Test search with programming-related query
TEST_F(SearchServiceTest, Search_ProgrammingQuery) {
  // Arrange
  std::string query = "C++ programming code";
  auto query_embedding = create_test_embedding_with_values({0.1f, 0.2f, 0.3f, 0.4f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 2);

  // Assert
  ASSERT_FALSE(results.empty());
  
  // Should find the C++ file
  bool found_cpp_file = false;
  for (const auto& result : results) {
    if (result.file.path.find("main.cpp") != std::string::npos) {
      found_cpp_file = true;
      break;
    }
  }
  EXPECT_TRUE(found_cpp_file);
}

// Test search with documentation query
TEST_F(SearchServiceTest, Search_DocumentationQuery) {
  // Arrange
  std::string query = "documentation and guides";
  auto query_embedding = create_test_embedding_with_values({0.5f, 0.5f, 0.5f, 0.5f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 2);

  // Assert
  ASSERT_FALSE(results.empty());
  
  // Should find documentation files
  bool found_docs_file = false;
  for (const auto& result : results) {
    if (result.file.path.find("README.md") != std::string::npos) {
      found_docs_file = true;
      break;
    }
  }
  EXPECT_TRUE(found_docs_file);
}

// Test search with default k value
TEST_F(SearchServiceTest, Search_DefaultKValue) {
  // Arrange
  std::string query = "test query";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 10);  // Default k is 10
}

// Test search with custom k value
TEST_F(SearchServiceTest, Search_CustomKValue) {
  // Arrange
  std::string query = "test query";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 1);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
}

// Test search with empty query
TEST_F(SearchServiceTest, Search_EmptyQuery) {
  // Arrange
  std::string query = "";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 3);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 3);
}

// Test search with very large k value
TEST_F(SearchServiceTest, Search_LargeKValue) {
  // Arrange
  std::string query = "test query";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 100);

  // Assert
  ASSERT_FALSE(results.empty());
  EXPECT_LE(results.size(), 4);  // Should not exceed available files
}

// Test search result ordering (by distance)
TEST_F(SearchServiceTest, Search_ResultsOrderedByDistance) {
  // Arrange
  std::string query = "machine learning";
  auto query_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 4);

  // Assert
  ASSERT_FALSE(results.empty());
  
  // Results should be ordered by distance (ascending)
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_LE(results[i-1].distance, results[i].distance);
  }
}

// Test search result structure
TEST_F(SearchServiceTest, Search_ResultStructure) {
  // Arrange
  std::string query = "test query";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 1);

  // Assert
  ASSERT_FALSE(results.empty());
  
  const auto& result = results[0];
  EXPECT_GT(result.id, 0);
  EXPECT_GE(result.distance, 0.0f);
  EXPECT_FALSE(result.file.path.empty());
  EXPECT_FALSE(result.file.content_hash.empty());
}

// Test error handling when Ollama client fails
TEST_F(SearchServiceTest, Search_OllamaClientError) {
  // Arrange
  std::string query = "test query";
  EXPECT_CALL(*mock_ollama_client_, get_embedding(query))
      .WillOnce(testing::Throw(magic_core::OllamaError("Embedding failed")));

  // Act & Assert
  EXPECT_THROW(search_service_->search_files(query, 3), std::exception);
}

// Test error handling when metadata store search fails
TEST_F(SearchServiceTest, Search_MetadataStoreError) {
  // Arrange
  std::string query = "test query";
  
  // Simulate metadata store error by returning an empty vector
  // This would cause an error in the Faiss search
  EXPECT_CALL(*mock_ollama_client_, get_embedding(query))
      .WillOnce(testing::Return(std::vector<float>()));  // Empty vector

  // Act & Assert
  EXPECT_THROW(search_service_->search_files(query, 3), std::exception);
}

// Test search with different embedding dimensions
TEST_F(SearchServiceTest, Search_DifferentEmbeddingDimensions) {
  // Arrange
  std::string query = "test query";
  std::vector<float> query_embedding(1024, 0.1f);  // Correct dimension
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 3);

  // Assert
  ASSERT_FALSE(results.empty());
}

// Test search with edge case values
TEST_F(SearchServiceTest, Search_EdgeCaseValues) {
  // Arrange
  std::string query = "test query";
  std::vector<float> query_embedding(1024, 0.0f);  // All zeros
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 3);

  // Assert
  ASSERT_FALSE(results.empty());
}

// Test search performance with multiple queries
TEST_F(SearchServiceTest, Search_MultipleQueries) {
  // Arrange
  std::vector<std::string> queries = {
    "machine learning",
    "programming code", 
    "documentation",
    "algorithms"
  };

  // Act & Assert
  for (const auto& query : queries) {
    auto query_embedding = create_test_embedding();
    setupQueryEmbeddingExpectation(query, query_embedding);
    
    auto results = search_service_->search_files(query, 2);
    EXPECT_FALSE(results.empty());
  }
}

// ===== NEW TESTS FOR CHUNK SEARCH FUNCTIONALITY =====

// Test the new search() method that returns both file and chunk results
TEST_F(SearchServiceTest, Search_CombinedFileAndChunkResults) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "machine learning algorithms";
  auto query_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search(query, 3);

  // Assert
  ASSERT_FALSE(results.file_results.empty());
  // Chunk results may be empty if no chunks match, but the structure should be valid
  EXPECT_LE(results.file_results.size(), 3);
  EXPECT_LE(results.chunk_results.size(), 3);
  
  // Verify file results structure
  for (const auto& file_result : results.file_results) {
    EXPECT_GT(file_result.id, 0);
    EXPECT_GE(file_result.distance, 0.0f);
    EXPECT_FALSE(file_result.file.path.empty());
  }
  
  // Verify chunk results structure (if any exist)
  for (const auto& chunk_result : results.chunk_results) {
    EXPECT_GT(chunk_result.id, 0);
    EXPECT_GE(chunk_result.distance, 0.0f);
    EXPECT_GT(chunk_result.file_id, 0);
    EXPECT_GE(chunk_result.chunk_index, 0);
    EXPECT_FALSE(chunk_result.content.empty());
  }
}

// Test search() method with default k value
TEST_F(SearchServiceTest, Search_CombinedDefaultKValue) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "test query";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search(query);

  // Assert
  ASSERT_FALSE(results.file_results.empty());
  EXPECT_LE(results.file_results.size(), 10);  // Default k is 10
  EXPECT_LE(results.chunk_results.size(), 10);
}

// Test search() method with custom k value
TEST_F(SearchServiceTest, Search_CombinedCustomKValue) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "test query";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search(query, 1);

  // Assert
  ASSERT_FALSE(results.file_results.empty());
  EXPECT_EQ(results.file_results.size(), 1);
  EXPECT_LE(results.chunk_results.size(), 1);
}

// Test search() method with empty query
TEST_F(SearchServiceTest, Search_CombinedEmptyQuery) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search(query, 3);

  // Assert
  ASSERT_FALSE(results.file_results.empty());
  EXPECT_LE(results.file_results.size(), 3);
  EXPECT_LE(results.chunk_results.size(), 3);
}

// Test search() method error handling
TEST_F(SearchServiceTest, Search_CombinedOllamaClientError) {
  // Arrange
  std::string query = "test query";
  EXPECT_CALL(*mock_ollama_client_, get_embedding(query))
      .WillOnce(testing::Throw(magic_core::OllamaError("Embedding failed")));

  // Act & Assert
  EXPECT_THROW(search_service_->search(query, 3), std::exception);
}

// Test search() method with metadata store error
TEST_F(SearchServiceTest, Search_CombinedMetadataStoreError) {
  // Arrange
  std::string query = "test query";
  
  // Simulate metadata store error by returning an empty vector
  EXPECT_CALL(*mock_ollama_client_, get_embedding(query))
      .WillOnce(testing::Return(std::vector<float>()));  // Empty vector

  // Act & Assert
  EXPECT_THROW(search_service_->search(query, 3), std::exception);
}

// Test that chunk results are properly ordered by distance
TEST_F(SearchServiceTest, Search_ChunkResultsOrderedByDistance) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "machine learning";
  auto query_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search(query, 5);

  // Assert
  ASSERT_FALSE(results.file_results.empty());
  
  // File results should be ordered by distance (ascending)
  for (size_t i = 1; i < results.file_results.size(); ++i) {
    EXPECT_LE(results.file_results[i-1].distance, results.file_results[i].distance);
  }
  
  // Chunk results should be ordered by distance (ascending) if any exist
  for (size_t i = 1; i < results.chunk_results.size(); ++i) {
    EXPECT_LE(results.chunk_results[i-1].distance, results.chunk_results[i].distance);
  }
}

// Test that chunk results contain valid metadata
TEST_F(SearchServiceTest, Search_ChunkResultsValidMetadata) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "machine learning";
  auto query_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search(query, 5);

  // Assert
  ASSERT_FALSE(results.file_results.empty());
  
  // If we have chunk results, verify their metadata
  for (const auto& chunk_result : results.chunk_results) {
    EXPECT_GT(chunk_result.id, 0);
    EXPECT_GT(chunk_result.file_id, 0);
    EXPECT_GE(chunk_result.chunk_index, 0);
    EXPECT_FALSE(chunk_result.content.empty());
    EXPECT_GE(chunk_result.distance, 0.0f);
  }
}

// Test that file IDs are correctly extracted from file results
TEST_F(SearchServiceTest, Search_FileIdsExtraction) {
  // Arrange
  std::string query = "test query";
  auto query_embedding = create_test_embedding();
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 3);

  // Assert
  ASSERT_FALSE(results.empty());
  
  // Verify that all file IDs are positive
  for (const auto& result : results) {
    EXPECT_GT(result.id, 0);
  }
}

// Test search with no matching files
TEST_F(SearchServiceTest, Search_NoMatchingFiles) {
  // Arrange
  std::string query = "completely unrelated query";
  auto query_embedding = create_test_embedding_with_values({0.0f, 0.0f, 0.0f, 0.0f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search_files(query, 3);

  // Assert
  // Should still return some results, but they might not be very relevant
  EXPECT_LE(results.size(), 3);
}

// Test search with no matching files for combined search
TEST_F(SearchServiceTest, Search_CombinedNoMatchingFiles) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "completely unrelated query";
  auto query_embedding = create_test_embedding_with_values({0.0f, 0.0f, 0.0f, 0.0f});
  setupQueryEmbeddingExpectation(query, query_embedding);

  // Act
  auto results = search_service_->search(query, 3);

  // Assert
  // Should still return some file results, but they might not be very relevant
  EXPECT_LE(results.file_results.size(), 3);
  EXPECT_LE(results.chunk_results.size(), 3);
}

// Test that the same query embedding is used for both file and chunk search
TEST_F(SearchServiceTest, Search_ConsistentQueryEmbedding) {
  // Arrange - Set up data with chunks
  setupTestDataWithChunks();
  
  std::string query = "machine learning";
  auto query_embedding = create_test_embedding_with_values({0.9f, 0.8f, 0.7f, 0.6f});
  
  // Expect the embedding to be called exactly once
  EXPECT_CALL(*mock_ollama_client_, get_embedding(query))
      .WillOnce(testing::Return(query_embedding));

  // Act
  auto results = search_service_->search(query, 3);

  // Assert
  ASSERT_FALSE(results.file_results.empty());
  // The embedding should be used for both file and chunk search
  // We can't directly verify this, but we can verify the results are consistent
  EXPECT_LE(results.file_results.size(), 3);
  EXPECT_LE(results.chunk_results.size(), 3);
}

}  // namespace magic_core