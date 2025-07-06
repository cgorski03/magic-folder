#pragma once

#include <gmock/gmock.h>

#include <memory>

#include "magic_core/content_extractor.hpp"
#include "magic_core/ollama_client.hpp"

namespace magic_tests {

/**
 * Mock class for OllamaClient to use in tests
 */
class MockOllamaClient : public magic_core::OllamaClient {
 public:
  MockOllamaClient() : magic_core::OllamaClient("http://localhost:11434", "mxbai-embed-large") {}

  MOCK_METHOD(std::vector<float>, get_embedding, (const std::string& text), (override));
};

/**
 * Mock class for ContentExtractor to use in tests
 */
class MockContentExtractor : public magic_core::ContentExtractor {
 public:
  MockContentExtractor() : magic_core::ContentExtractor() {}

  MOCK_METHOD(magic_core::ExtractedContent,
              extract_content,
              (const std::filesystem::path& file_path),
              (override));
  MOCK_METHOD(magic_core::ExtractedContent,
              extract_from_text,
              (const std::string& text, const std::string& filename),
              (override));
  MOCK_METHOD(magic_core::FileType,
              detect_file_type,
              (const std::filesystem::path& file_path),
              (override));
  MOCK_METHOD(bool, is_supported_file_type, (const std::filesystem::path& file_path), (override));
  MOCK_METHOD(std::vector<std::string>, get_supported_extensions, (), (const, override));
};

/**
 * Utility functions for creating test data in tests
 */
namespace MockUtilities {

// Create a test ExtractedContent object
inline magic_core::ExtractedContent create_test_extracted_content(
    const std::string& text_content = "This is test content for processing.",
    const std::string& title = "Test File",
    magic_core::FileType file_type = magic_core::FileType::Text,
    const std::string& content_hash = "test_hash_123") {
  magic_core::ExtractedContent content;
  content.text_content = text_content;
  content.title = title;
  content.keywords = {"test", "content", "processing"};
  content.file_type = file_type;
  content.word_count = 7;
  content.content_hash = content_hash;
  return content;
}

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

}  // namespace MockUtilities

}  // namespace magic_tests