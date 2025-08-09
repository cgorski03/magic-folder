#pragma once

#include <gmock/gmock.h>

#include "magic_core/extractors/content_extractor.hpp"
#include "magic_core/extractors/content_extractor_factory.hpp"
#include "magic_core/db/metadata_store.hpp"
#include "magic_core/llm/ollama_client.hpp"
#include "magic_core/types/chunk.hpp"

namespace magic_tests {

/**
 * Mock class for OllamaClient to use in tests
 */
class MockOllamaClient : public magic_core::OllamaClient {
 public:
  MockOllamaClient() : magic_core::OllamaClient("http://localhost:11434", "mxbai-embed-large") {
    // Set default behavior to return a valid embedding vector
    std::vector<float> default_embedding(1024, 0.1f);
    default_embedding[0] = 0.5f;
    default_embedding[100] = 0.3f;
    default_embedding[500] = 0.7f;
    
    ON_CALL(*this, get_embedding(testing::_))
        .WillByDefault(testing::Return(default_embedding));
  }

  MOCK_METHOD(std::vector<float>, get_embedding, (const std::string& text), (override));
};

/**
 * Mock class for ContentExtractor to use in tests
 */
class MockContentExtractor : public magic_core::ContentExtractor {
 public:
  MockContentExtractor() = default;

  MOCK_METHOD(bool, can_handle, (const std::filesystem::path& file_path), (const, override));
  MOCK_METHOD(std::vector<Chunk>, get_chunks, (const std::filesystem::path& file_path), (const, override));
  MOCK_METHOD(magic_core::ExtractionResult, extract_with_hash, (const std::filesystem::path& file_path), (const, override));
  MOCK_METHOD(magic_core::FileType, get_file_type, (), (const, override));
  // Note: get_content_hash is not virtual in the base class, so we can't mock it directly
};

/**
 * Mock class for ContentExtractorFactory to use in tests
 */
class MockContentExtractorFactory : public magic_core::ContentExtractorFactory {
 public:
  MockContentExtractorFactory() = default;

  MOCK_METHOD(const magic_core::ContentExtractor&, get_extractor_for, 
              (const std::filesystem::path& file_path), (const, override));
};

/**
 * Utility functions for creating test data in tests
 */
namespace MockUtilities {

// Create a test embedding vector
inline std::vector<float> create_test_embedding(float value = 0.1f, size_t dimensions = 1024) {
  std::vector<float> embedding(dimensions, value);
  embedding[0] = 0.5f;
  embedding[100] = 0.3f;
  embedding[500] = 0.7f;
  return embedding;
}

// Create a test embedding with custom values
inline std::vector<float> create_test_embedding_with_values(const std::vector<float>& values) {
  std::vector<float> embedding(1024, 0.1f);
  for (size_t i = 0; i < values.size() && i < embedding.size(); ++i) {
    embedding[i] = values[i];
  }
  return embedding;
}

// Create a test Chunk object
inline Chunk create_test_chunk(
    const std::string& content = "This is test chunk content for processing.",
    int chunk_index = 0) {
  Chunk chunk;
  chunk.content = content;
  chunk.chunk_index = chunk_index;
  return chunk;
}

// Create multiple test chunks
inline std::vector<Chunk> create_test_chunks(
    int count = 3,
    const std::string& base_content = "Test chunk content") {
  std::vector<Chunk> chunks;
  chunks.reserve(count);
  
  for (int i = 0; i < count; ++i) {
    chunks.push_back(create_test_chunk(
        base_content + " " + std::to_string(i), i));
  }
  
  return chunks;
}

// Create a test Chunk with embedding
inline Chunk create_test_chunk_with_embedding(
    const std::string& content = "This is test chunk content.",
    int chunk_index = 0,
    float embedding_base_value = 0.1f) {
  Chunk chunk;
  chunk.content = content;
  chunk.chunk_index = chunk_index;
  chunk.vector_embedding = create_test_embedding(embedding_base_value);
  return chunk;
}

// Create multiple test chunks with embeddings
inline std::vector<Chunk> create_test_chunks_with_embeddings(
    int count = 3,
    const std::string& base_content = "Test chunk content") {
  std::vector<Chunk> chunks;
  chunks.reserve(count);
  
  for (int i = 0; i < count; ++i) {
    chunks.push_back(create_test_chunk_with_embedding(
        base_content + " " + std::to_string(i), i, 0.1f + (0.1f * i)));
  }
  
  return chunks;
}

// Create test BasicFileMetadata for the new workflow
inline magic_core::BasicFileMetadata create_test_basic_metadata(
    const std::string& path,
    const std::string& content_hash = "test_hash_123",
    magic_core::FileType file_type = magic_core::FileType::Text,
    size_t file_size = 1024,
    magic_core::ProcessingStatus processing_status = magic_core::ProcessingStatus::PROCESSING) {
  
  magic_core::BasicFileMetadata metadata;
  metadata.path = path;
  metadata.original_path = path;
  metadata.content_hash = content_hash;
  metadata.file_type = file_type;
  metadata.file_size = file_size;
  metadata.processing_status = processing_status;
  
  auto now = std::chrono::system_clock::now();
  metadata.last_modified = now;
  metadata.created_at = now - std::chrono::hours(1);
  
  return metadata;
}

// Create test FileMetadata with AI analysis fields
inline magic_core::FileMetadata create_test_complete_metadata(
    const std::string& path,
    const std::string& content_hash = "test_hash_123",
    magic_core::FileType file_type = magic_core::FileType::Text,
    size_t file_size = 1024,
    bool include_vector = false,
    const std::string& suggested_category = "document",
    const std::string& suggested_filename = "test_file.txt") {
  
  magic_core::FileMetadata metadata;
  metadata.path = path;
  metadata.original_path = path;
  metadata.content_hash = content_hash;
  metadata.file_type = file_type;
  metadata.file_size = file_size;
  metadata.processing_status = magic_core::ProcessingStatus::PROCESSED;
  metadata.suggested_category = suggested_category;
  metadata.suggested_filename = suggested_filename;
  
  auto now = std::chrono::system_clock::now();
  metadata.last_modified = now;
  metadata.created_at = now - std::chrono::hours(1);
  
  if (include_vector) {
    metadata.summary_vector_embedding = create_test_embedding();
  }
  
  return metadata;
}

}  // namespace MockUtilities

}  // namespace magic_tests