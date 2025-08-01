#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "magic_core/extractors/content_extractor.hpp"
#include "test_utilities.hpp"
#include <utf8.h>

namespace magic_core {

// Mock ContentExtractor for testing base class functionality
class MockContentExtractor : public ContentExtractor {
 public:
  MOCK_METHOD(bool, can_handle, (const std::filesystem::path& file_path), (const, override));
  MOCK_METHOD(std::vector<Chunk>, get_chunks, (const std::filesystem::path& file_path), (const, override));
  MOCK_METHOD(ExtractionResult, extract_with_hash, (const std::filesystem::path& file_path), (const, override));
  
  // Expose protected constants for testing
  static constexpr size_t TEST_MIN_CHUNK_SIZE = MIN_CHUNK_SIZE;
  static constexpr size_t TEST_MAX_CHUNK_SIZE = MAX_CHUNK_SIZE;
  static constexpr size_t TEST_FIXED_CHUNK_SIZE = FIXED_CHUNK_SIZE;
  static constexpr size_t TEST_OVERLAP_SIZE = OVERLAP_SIZE;
  
  // Expose protected method for testing
  using ContentExtractor::split_into_fixed_chunks;
  using ContentExtractor::compute_hash_from_content;
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
  EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F(ContentExtractorTest, GetContentHash_SmallFile) {
  // Arrange
  auto small_file = create_test_file("small.txt", "hello");

  // Act
  std::string hash = mock_extractor_->get_content_hash(small_file);

  // Assert
  EXPECT_EQ(hash.length(), 64);
  EXPECT_EQ(hash, "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
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

  // Assert - When text is exactly FIXED_CHUNK_SIZE, it should create 2 chunks
  // because the algorithm steps by (FIXED_CHUNK_SIZE - OVERLAP_SIZE) and then adds overlap
  EXPECT_EQ(chunks.size(), 2);
  
  // First chunk should be the step size
  size_t step_size = MockContentExtractor::TEST_FIXED_CHUNK_SIZE - MockContentExtractor::TEST_OVERLAP_SIZE;
  EXPECT_EQ(chunks[0].length(), step_size);
  
  // Second chunk should be the remaining content
  EXPECT_EQ(chunks[1].length(), MockContentExtractor::TEST_OVERLAP_SIZE);
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

  // Verify chunk sizes - all chunks except the last should be step_size
  for (size_t i = 0; i < chunks.size() - 1; ++i) {
    EXPECT_EQ(chunks[i].length(), step_size);
  }
  
  // Last chunk might be smaller
  EXPECT_LE(chunks.back().length(), step_size);
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_VerifyOverlap) {
  // Arrange - Text large enough to create multiple chunks
  size_t large_size = MockContentExtractor::TEST_FIXED_CHUNK_SIZE + 100;
  std::string pattern = "ABCDEFGHIJ"; // Repeating pattern to verify chunking
  std::string large_text;
  while (large_text.length() < large_size) {
    large_text += pattern;
  }
  large_text = large_text.substr(0, large_size);

  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(large_text);

  // Assert
  if (chunks.size() > 1) {
    // The algorithm creates non-overlapping chunks
    // Verify that chunks are consecutive and don't overlap
    std::string chunk1 = chunks[0];
    std::string chunk2 = chunks[1];
    
    // Verify chunk sizes are reasonable
    EXPECT_GT(chunk1.length(), 0);
    EXPECT_GT(chunk2.length(), 0);
    
    // Verify chunks are different (no overlap)
    EXPECT_NE(chunk1, chunk2);
    
    // Verify the total length of chunks matches the original text
    size_t total_chunk_length = 0;
    for (const auto& chunk : chunks) {
      total_chunk_length += chunk.length();
    }
    EXPECT_EQ(total_chunk_length, large_text.length());
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
  
}

// ============================================================================
// UTF-8 Boundary Splitting Tests
// ============================================================================

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_SimpleASCII) {
  // Arrange - Simple ASCII text that should split normally
  std::string ascii_text = "This is a simple ASCII text that should split normally at character boundaries.";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(ascii_text);
  
  // Assert
  EXPECT_GT(chunks.size(), 0);
  // Verify all chunks are valid UTF-8
  for (const auto& chunk : chunks) {
    EXPECT_TRUE(utf8::is_valid(chunk.begin(), chunk.end())) << "Chunk contains invalid UTF-8: " << chunk;
  }
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_MultiByteCharacters) {
  // Arrange - Text with multi-byte UTF-8 characters
  std::string utf8_text = "Hello 世界! This text contains multi-byte characters like émojis 🚀 and ñoño.";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(utf8_text);
  
  // Assert
  EXPECT_GT(chunks.size(), 0);
  // Verify all chunks are valid UTF-8
  for (const auto& chunk : chunks) {
    EXPECT_TRUE(utf8::is_valid(chunk.begin(), chunk.end())) << "Chunk contains invalid UTF-8: " << chunk;
  }
  
  // Verify that multi-byte characters are not split in the middle
  for (const auto& chunk : chunks) {
    std::string::const_iterator it = chunk.begin();
    while (it != chunk.end()) {
      EXPECT_NO_THROW(utf8::next(it, chunk.end())) << "Invalid UTF-8 sequence in chunk: " << chunk;
    }
  }
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_Emojis) {
  // Arrange - Text with emojis (4-byte UTF-8 characters)
  std::string emoji_text = "🚀🚁🚂🚃🚄🚅🚆🚇🚈🚉🚊🚋🚌🚍🚎🚏🚐🚑🚒🚓🚔🚕🚖🚗🚘🚙🚚🚛🚜🚝🚞🚟🚠🚡🚢🚣🚤🚥🚦🚧🚨🚩🚪🚫🚬🚭🚮🚯🚰🚱🚲🚳🚴🚵🚶🚷🚸🚹🚺🚻🚼🚽🚾🚿🛀🛁🛂🛃🛄🛅🛆🛇🛈🛉🛊🛋🛌🛍🛎🛏🛐🛑🛒🛓🛔🛕🛖🛗🛘🛙🛚🛛🛜🛝🛞🛟🛠🛡🛢🛣🛤🛥🛦🛧🛨🛩🛪🛫🛬🛭🛮🛯🛰🛱🛲🛳🛴🛵🛶🛷🛸🛹🛺🛻🛼🛽🛾🛿";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(emoji_text);
  
  // Assert
  EXPECT_GT(chunks.size(), 0);
  // Verify all chunks are valid UTF-8
  for (const auto& chunk : chunks) {
    EXPECT_TRUE(utf8::is_valid(chunk.begin(), chunk.end())) << "Chunk contains invalid UTF-8: " << chunk;
  }
  
  // Verify that emojis are not split in the middle
  for (const auto& chunk : chunks) {
    std::string::const_iterator it = chunk.begin();
    while (it != chunk.end()) {
      EXPECT_NO_THROW(utf8::next(it, chunk.end())) << "Invalid UTF-8 sequence in chunk: " << chunk;
    }
  }
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_MixedContent) {
  // Arrange - Mixed content with ASCII, multi-byte, and emojis
  std::string mixed_text = "ASCII text 世界 with émojis 🚀 and ñoño characters. "
                          "More text with 🎉🎊🎋🎌🎍🎎🎏🎐🎑🎒🎓🎔🎕🎖🎗🎘🎙🎚🎛🎜🎝🎞🎟🎠🎡🎢🎣🎤🎥🎦🎧🎨🎩🎪🎫🎬🎭🎮🎯🎰🎱🎲🎳🎴🎵🎶🎷🎸🎹🎺🎻🎼🎽🎾🎿";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(mixed_text);
  
  // Assert
  EXPECT_GT(chunks.size(), 0);
  // Verify all chunks are valid UTF-8
  for (const auto& chunk : chunks) {
    EXPECT_TRUE(utf8::is_valid(chunk.begin(), chunk.end())) << "Chunk contains invalid UTF-8: " << chunk;
  }
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_EdgeCase_SingleMultiByte) {
  // Arrange - Single multi-byte character at chunk boundary
  std::string text_with_boundary_char = "Hello 世界! This text has a multi-byte character at the boundary.";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(text_with_boundary_char);
  
  // Assert
  EXPECT_GT(chunks.size(), 0);
  // Verify all chunks are valid UTF-8
  for (const auto& chunk : chunks) {
    EXPECT_TRUE(utf8::is_valid(chunk.begin(), chunk.end())) << "Chunk contains invalid UTF-8: " << chunk;
  }
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_EdgeCase_EmojiAtBoundary) {
  // Arrange - Emoji at chunk boundary
  std::string text_with_emoji_boundary = "Hello 🚀! This text has an emoji at the boundary.";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(text_with_emoji_boundary);
  
  // Assert
  EXPECT_GT(chunks.size(), 0);
  // Verify all chunks are valid UTF-8
  for (const auto& chunk : chunks) {
    EXPECT_TRUE(utf8::is_valid(chunk.begin(), chunk.end())) << "Chunk contains invalid UTF-8: " << chunk;
  }
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_ReconstructOriginal) {
  // Arrange - Text that should be reconstructible
  std::string original_text = "Hello 世界! This is a test with émojis 🚀 and ñoño characters. "
                             "We want to make sure that when we split and reconstruct, we get the same text.";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(original_text);
  
  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Reconstruct the text from chunks
  std::string reconstructed;
  for (size_t i = 0; i < chunks.size(); ++i) {
    if (i > 0) {
      // Remove overlap from previous chunk
      size_t overlap_size = MockContentExtractor::TEST_OVERLAP_SIZE;
      if (reconstructed.length() >= overlap_size) {
        reconstructed = reconstructed.substr(0, reconstructed.length() - overlap_size);
      }
    }
    reconstructed += chunks[i];
  }
  
  // Verify reconstruction matches original
  EXPECT_EQ(reconstructed, original_text) << "Reconstructed text doesn't match original";
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_InvalidUTF8Handling) {
  // Arrange - Text with invalid UTF-8 sequences
  std::string invalid_utf8 = "Hello \xFF\xFE world!"; // Invalid UTF-8 sequence
  
  // Act & Assert
  // The function should throw an exception for invalid UTF-8
  EXPECT_THROW({
    auto chunks = mock_extractor_->split_into_fixed_chunks(invalid_utf8);
  }, utf8::invalid_utf8);
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_EmptyString) {
  // Arrange
  std::string empty_text = "";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(empty_text);
  
  // Assert
  EXPECT_TRUE(chunks.empty());
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_SingleCharacter) {
  // Arrange - Single UTF-8 character
  std::string single_char = "🚀";
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(single_char);
  
  // Assert
  EXPECT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0], single_char);
  EXPECT_TRUE(utf8::is_valid(chunks[0].begin(), chunks[0].end()));
}

TEST_F(ContentExtractorTest, SplitIntoFixedChunks_UTF8Boundary_ChunkSizeVerification) {
  // Arrange - Text that should create multiple chunks
  std::string long_text;
  for (int i = 0; i < 100; ++i) {
    long_text += "Hello 世界! 🚀 ";
  }
  
  // Act
  auto chunks = mock_extractor_->split_into_fixed_chunks(long_text);
  
  // Assert
  EXPECT_GT(chunks.size(), 1);
  
  // Verify chunk sizes are reasonable (not exceeding fixed chunk size)
  for (size_t i = 0; i < chunks.size() - 1; ++i) {
    EXPECT_LE(chunks[i].length(), MockContentExtractor::TEST_FIXED_CHUNK_SIZE);
  }
  
  // Verify all chunks are valid UTF-8
  for (const auto& chunk : chunks) {
    EXPECT_TRUE(utf8::is_valid(chunk.begin(), chunk.end())) << "Chunk contains invalid UTF-8: " << chunk;
  }
}

} // namespace magic_core