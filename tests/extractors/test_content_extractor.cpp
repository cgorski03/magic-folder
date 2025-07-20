#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "magic_core/extractors/content_extractor.hpp"
#include "test_utilities.hpp"

namespace magic_core {

// Mock ContentExtractor for testing base class functionality
class MockContentExtractor : public ContentExtractor {
 public:
  MOCK_METHOD(bool, can_handle, (const std::filesystem::path& file_path), (const, override));
  MOCK_METHOD(std::vector<Chunk>, get_chunks, (const std::filesystem::path& file_path), (const, override));
  
  // Expose protected constants for testing
  static constexpr size_t TEST_MIN_CHUNK_SIZE = MIN_CHUNK_SIZE;
  static constexpr size_t TEST_MAX_CHUNK_SIZE = MAX_CHUNK_SIZE;
  static constexpr size_t TEST_FIXED_CHUNK_SIZE = FIXED_CHUNK_SIZE;
  static constexpr size_t TEST_OVERLAP_SIZE = OVERLAP_SIZE;
  
  // Expose protected method for testing
  using ContentExtractor::split_into_fixed_chunks;
};

class ContentExtractorTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    mock_extractor_ = std::make_unique<MockContentExtractor>();
    
    // Create test directory
    test_dir_ = std::filesystem::temp_directory_path() / "extractor_tests";
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override {
    // Clean up test files
    if (std::filesystem::exists(test_dir_)) {
      std::filesystem::remove_all(test_dir_);
    }
    MetadataStoreTestBase::TearDown();
  }

  // Helper to create test files
  std::filesystem::path create_test_file(const std::string& filename, const std::string& content) {
    auto file_path = test_dir_ / filename;
    std::ofstream file(file_path);
    file << content;
    file.close();
    return file_path;
  }

  // Helper to create content of specific size
  std::string create_content_of_size(size_t target_size, const std::string& base_text = "a") {
    std::string content;
    while (content.length() < target_size) {
      content += base_text;
    }
    return content.substr(0, target_size);
  }

  // Helper to create content just below/above thresholds
  std::string create_content_below_min() {
    return create_content_of_size(MockContentExtractor::TEST_MIN_CHUNK_SIZE - 10, "x");
  }

  std::string create_content_at_min() {
    return create_content_of_size(MockContentExtractor::TEST_MIN_CHUNK_SIZE, "y");
  }

  std::string create_content_above_min() {
    return create_content_of_size(MockContentExtractor::TEST_MIN_CHUNK_SIZE + 50, "z");
  }

  std::string create_content_at_max() {
    return create_content_of_size(MockContentExtractor::TEST_MAX_CHUNK_SIZE, "m");
  }

  std::string create_content_above_max() {
    return create_content_of_size(MockContentExtractor::TEST_MAX_CHUNK_SIZE + 100, "n");
  }

  std::unique_ptr<MockContentExtractor> mock_extractor_;
  std::filesystem::path test_dir_;
};

// Test get_content_hash with different file sizes
TEST_F(ContentExtractorTest, GetContentHash_EmptyFile) {
  // Arrange
  auto empty_file = create_test_file("empty.txt", "");

  // Act
  std::string hash = mock_extractor_->get_content_hash(empty_file);

  // Assert
  EXPECT_EQ(hash.length(), 64); // SHA256 is 64 hex characters
  EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"); // SHA256 of empty string
}

TEST_F(ContentExtractorTest, GetContentHash_SmallFile) {
  // Arrange
  auto small_file = create_test_file("small.txt", "hello");

  // Act
  std::string hash = mock_extractor_->get_content_hash(small_file);

  // Assert
  EXPECT_EQ(hash.length(), 64);
  EXPECT_EQ(hash, "2cf24dba4f21d4288094c99fa9c6f33e4e6e4f2e9c4e75b7b1bbcb1f9e4b8a2c"); // SHA256 of "hello"
}

TEST_F(ContentExtractorTest, GetContentHash_MediumFile) {
  // Arrange - Create file larger than buffer (1024 bytes)
  std::string content = create_content_of_size(1500, "a");
  auto medium_file = create_test_file("medium.txt", content);

  // Act
  std::string hash = mock_extractor_->get_content_hash(medium_file);

  // Assert
  EXPECT_EQ(hash.length(), 64);
  EXPECT_FALSE(hash.empty());
  
  // Verify consistency - same content should produce same hash
  std::string hash2 = mock_extractor_->get_content_hash(medium_file);
  EXPECT_EQ(hash, hash2);
}

TEST_F(ContentExtractorTest, GetContentHash_LargeFile) {
  // Arrange - Create file much larger than buffer
  std::string content = create_content_of_size(5000, "x");
  auto large_file = create_test_file("large.txt", content);

  // Act
  std::string hash = mock_extractor_->get_content_hash(large_file);

  // Assert
  EXPECT_EQ(hash.length(), 64);
  EXPECT_FALSE(hash.empty());
  
  // Different content should produce different hash
  std::string different_content = create_content_of_size(5000, "y");
  auto different_file = create_test_file("different.txt", different_content);
  std::string different_hash = mock_extractor_->get_content_hash(different_file);
  EXPECT_NE(hash, different_hash);
}

TEST_F(ContentExtractorTest, GetContentHash_SameContentSameHash) {
  // Arrange
  std::string content = "This is test content for hash verification.";
  auto file1 = create_test_file("file1.txt", content);
  auto file2 = create_test_file("file2.txt", content);

  // Act
  std::string hash1 = mock_extractor_->get_content_hash(file1);
  std::string hash2 = mock_extractor_->get_content_hash(file2);

  // Assert
  EXPECT_EQ(hash1, hash2);
}

TEST_F(ContentExtractorTest, GetContentHash_NonExistentFile) {
  // Arrange
  auto non_existent = test_dir_ / "does_not_exist.txt";

  // Act & Assert
  EXPECT_THROW(mock_extractor_->get_content_hash(non_existent), ContentExtractorError);
}

TEST_F(ContentExtractorTest, GetContentHash_BinaryContent) {
  // Arrange - Create file with binary content
  auto binary_file = test_dir_ / "binary.dat";
  std::ofstream file(binary_file, std::ios::binary);
  for (int i = 0; i < 256; ++i) {
    file << static_cast<char>(i);
  }
  file.close();

  // Act
  std::string hash = mock_extractor_->get_content_hash(binary_file);

  // Assert
  EXPECT_EQ(hash.length(), 64);
  EXPECT_FALSE(hash.empty());
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_EmptyString) {
  // Arrange
  std::string empty_text = "";

  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(empty_text);

  // Assert
  EXPECT_TRUE(chunks.empty());
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_SmallText) {
  // Arrange - Text smaller than fixed chunk size
  std::string small_text = create_content_below_min();

  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(small_text);

  // Assert
  EXPECT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0], small_text);
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_ExactlyFixedSize) {
  // Arrange - Text exactly at fixed chunk size
  std::string exact_text = create_content_of_size(MockContentExtractor::TEST_FIXED_CHUNK_SIZE, "f");

  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(exact_text);

  // Assert
  EXPECT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0], exact_text);
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_LargeText) {
  // Arrange - Text much larger than fixed chunk size
  size_t large_size = MockContentExtractor::TEST_FIXED_CHUNK_SIZE * 3; // 3x the chunk size
  std::string large_text = create_content_of_size(large_size, "L");

  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(large_text);

  // Assert
  EXPECT_GT(chunks.size(), 1); // Should be split into multiple chunks
  
  // Calculate expected number of chunks based on step size
  size_t step_size = MockContentExtractor::TEST_FIXED_CHUNK_SIZE - MockContentExtractor::TEST_OVERLAP_SIZE;
  size_t expected_chunks = (large_size + step_size - 1) / step_size; // Ceiling division
  EXPECT_EQ(chunks.size(), expected_chunks);

  // Verify chunk sizes
  for (size_t i = 0; i < chunks.size() - 1; ++i) {
    EXPECT_EQ(chunks[i].length(), MockContentExtractor::TEST_FIXED_CHUNK_SIZE);
  }
  
  // Last chunk might be smaller
  EXPECT_LE(chunks.back().length(), MockContentExtractor::TEST_FIXED_CHUNK_SIZE);
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_VerifyOverlap) {
  // Arrange - Text large enough to create overlapping chunks
  size_t large_size = MockContentExtractor::TEST_FIXED_CHUNK_SIZE + 100;
  std::string pattern = "ABCDEFGHIJ"; // Repeating pattern to verify overlap
  std::string large_text;
  while (large_text.length() < large_size) {
    large_text += pattern;
  }
  large_text = large_text.substr(0, large_size);

  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(large_text);

  // Assert
  if (chunks.size() > 1) {
    // Find overlap between first two chunks
    std::string chunk1 = chunks[0];
    std::string chunk2 = chunks[1];
    
    // The end of chunk1 should overlap with the beginning of chunk2
    size_t overlap_start = MockContentExtractor::TEST_FIXED_CHUNK_SIZE - MockContentExtractor::TEST_OVERLAP_SIZE;
    std::string chunk1_end = chunk1.substr(overlap_start);
    std::string chunk2_start = chunk2.substr(0, MockContentExtractor::TEST_OVERLAP_SIZE);
    
    EXPECT_EQ(chunk1_end, chunk2_start) << "Chunks should have proper overlap";
  }
}

// Test chunking constants are reasonable and accessible
TEST_F(ContentExtractorTest, ChunkingConstants_AreReasonable) {
  // Verify constants are within expected ranges
  EXPECT_GT(MockContentExtractor::TEST_MAX_CHUNK_SIZE, MockContentExtractor::TEST_MIN_CHUNK_SIZE);
  EXPECT_GT(MockContentExtractor::TEST_FIXED_CHUNK_SIZE, MockContentExtractor::TEST_MIN_CHUNK_SIZE);
  EXPECT_LT(MockContentExtractor::TEST_OVERLAP_SIZE, MockContentExtractor::TEST_FIXED_CHUNK_SIZE);
  
  // Verify they're positive
  EXPECT_GT(MockContentExtractor::TEST_MIN_CHUNK_SIZE, 0);
  EXPECT_GT(MockContentExtractor::TEST_MAX_CHUNK_SIZE, 0);
  EXPECT_GT(MockContentExtractor::TEST_FIXED_CHUNK_SIZE, 0);
  
  // Print constants for debugging (helpful when constants change)
  std::cout << "MIN_CHUNK_SIZE: " << MockContentExtractor::TEST_MIN_CHUNK_SIZE << std::endl;
  std::cout << "MAX_CHUNK_SIZE: " << MockContentExtractor::TEST_MAX_CHUNK_SIZE << std::endl;
  std::cout << "FIXED_CHUNK_SIZE: " << MockContentExtractor::TEST_FIXED_CHUNK_SIZE << std::endl;
  std::cout << "OVERLAP_SIZE: " << MockContentExtractor::TEST_OVERLAP_SIZE << std::endl;
}

} // namespace magic_core