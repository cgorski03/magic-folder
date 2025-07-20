#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "magic_core/extractors/plaintext_extractor.hpp"
#include "test_utilities.hpp"

namespace magic_core {

class PlainTextExtractorTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    extractor_ = std::make_unique<PlainTextExtractor>();
    
    // Create test directory
    test_dir_ = std::filesystem::temp_directory_path() / "plaintext_tests";
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

  // Helper class to access protected constants
  class TestableExtractor : public PlainTextExtractor {
  public:
    static constexpr size_t MIN_SIZE = MIN_CHUNK_SIZE;
    static constexpr size_t MAX_SIZE = MAX_CHUNK_SIZE;  
    static constexpr size_t FIXED_SIZE = FIXED_CHUNK_SIZE;
  };

  // Helper to create content of specific size
  std::string create_content_of_size(size_t target_size, const std::string& base_text = "a") {
    std::string content;
    while (content.length() < target_size) {
      content += base_text;
    }
    return content.substr(0, target_size);
  }

  // Helper to create paragraphs of specific sizes
  std::string create_paragraph(size_t content_size, char fill_char = 'p') {
    return create_content_of_size(content_size, std::string(1, fill_char));
  }

  std::string create_paragraphs_with_breaks(const std::vector<size_t>& paragraph_sizes, 
                                           const std::vector<char>& fill_chars = {}) {
    std::string content;
    for (size_t i = 0; i < paragraph_sizes.size(); ++i) {
      char fill = (i < fill_chars.size()) ? fill_chars[i] : ('a' + static_cast<char>(i));
      content += create_paragraph(paragraph_sizes[i], fill);
      if (i < paragraph_sizes.size() - 1) {
        content += "\n\n"; // Paragraph separator
      }
    }
    return content;
  }

  std::unique_ptr<PlainTextExtractor> extractor_;
  std::filesystem::path test_dir_;
};

// Test can_handle method
TEST_F(PlainTextExtractorTest, CanHandle_TextFiles) {
  EXPECT_TRUE(extractor_->can_handle("/path/to/file.txt"));
  EXPECT_TRUE(extractor_->can_handle("/path/to/README.txt"));
  EXPECT_TRUE(extractor_->can_handle("document.txt"));
}

TEST_F(PlainTextExtractorTest, CanHandle_NonTextFiles) {
  EXPECT_FALSE(extractor_->can_handle("/path/to/file.md"));
  EXPECT_FALSE(extractor_->can_handle("/path/to/file.doc"));
  EXPECT_FALSE(extractor_->can_handle("/path/to/file.pdf"));
  EXPECT_FALSE(extractor_->can_handle("/path/to/file"));
  EXPECT_FALSE(extractor_->can_handle("file.TXT")); // Case sensitive
}

// Test chunking with empty file
TEST_F(PlainTextExtractorTest, GetChunks_EmptyFile) {
  // Arrange
  auto empty_file = create_test_file("empty.txt", "");

  // Act
  auto chunks = extractor_->get_chunks(empty_file);

  // Assert
  EXPECT_TRUE(chunks.empty());
}

// Test single paragraph below minimum size
TEST_F(PlainTextExtractorTest, GetChunks_SingleSmallParagraph) {
  // Arrange - Single paragraph below MIN_SIZE
  size_t small_size = TestableExtractor::MIN_SIZE / 2;
  std::string content = create_paragraph(small_size, 's');
  auto file = create_test_file("single_small.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0].chunk_index, 0);
  EXPECT_EQ(chunks[0].content, content);
}

// Test single paragraph at exactly minimum size
TEST_F(PlainTextExtractorTest, GetChunks_SingleParagraphAtMinSize) {
  // Arrange - Single paragraph exactly at MIN_SIZE
  std::string content = create_paragraph(TestableExtractor::MIN_SIZE, 'm');
  auto file = create_test_file("single_min.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0].chunk_index, 0);
  EXPECT_EQ(chunks[0].content, content);
}

// Test multiple small paragraphs that should merge
TEST_F(PlainTextExtractorTest, GetChunks_SmallParagraphs_MergingBehavior) {
  // Arrange - Multiple paragraphs that individually are small but together exceed MIN_SIZE
  size_t para_size = TestableExtractor::MIN_SIZE / 4; // Each paragraph is 1/4 of minimum
  std::vector<size_t> sizes = {para_size, para_size, para_size, para_size / 2}; // 4 small paragraphs
  std::string content = create_paragraphs_with_breaks(sizes, {'a', 'b', 'c', 'd'});
  
  auto file = create_test_file("small_paragraphs.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  // First 3 paragraphs should merge (together they exceed MIN_SIZE)
  // Last paragraph should be separate due to "last section" rule
  EXPECT_EQ(chunks.size(), 2) << "Should merge paragraphs until MIN_SIZE, last paragraph separate. MIN_SIZE=" << TestableExtractor::MIN_SIZE;
  
  EXPECT_GE(chunks[0].content.length(), TestableExtractor::MIN_SIZE) << "First chunk should be at least MIN_SIZE";
  // Last chunk might be below MIN_SIZE due to last section rule
  
  // Verify chunk indices
  for (size_t i = 0; i < chunks.size(); ++i) {
    EXPECT_EQ(chunks[i].chunk_index, static_cast<int>(i));
  }
}

// Test paragraph that exceeds maximum size
TEST_F(PlainTextExtractorTest, GetChunks_LargeParagraph_SplittingBehavior) {
  // Arrange - Single paragraph that exceeds MAX_SIZE and needs splitting
  size_t large_size = TestableExtractor::MAX_SIZE + (TestableExtractor::MAX_SIZE / 2); // 1.5x max size
  std::string large_paragraph = create_paragraph(large_size, 'L');
  std::string small_paragraph = create_paragraph(TestableExtractor::MIN_SIZE / 2, 's');
  std::string content = large_paragraph + "\n\n" + small_paragraph;
  
  auto file = create_test_file("large_paragraph.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 1) << "Large paragraph should be split. MAX_SIZE=" << TestableExtractor::MAX_SIZE;
  
  // At least one chunk should contain part of the large content
  bool found_large_content = false;
  for (const auto& chunk : chunks) {
    if (chunk.content.find(std::string(50, 'L')) != std::string::npos) {
      found_large_content = true;
      break;
    }
  }
  EXPECT_TRUE(found_large_content) << "Should find large paragraph content in chunks";
}

// Test last paragraph behavior
TEST_F(PlainTextExtractorTest, GetChunks_LastParagraphBehavior) {
  // Arrange - Test that last paragraph gets its own chunk even if small
  std::string large_paragraph = create_paragraph(TestableExtractor::MIN_SIZE + 50, 'X');
  std::string tiny_last = "Tiny final paragraph."; // Very small last paragraph
  std::string content = large_paragraph + "\n\n" + tiny_last;
  
  auto file = create_test_file("last_paragraph.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GE(chunks.size(), 2) << "Should have at least 2 chunks: large paragraph + tiny last";
  
  // Last chunk should exist even though it's small
  EXPECT_LT(chunks.back().content.length(), TestableExtractor::MIN_SIZE) << "Last chunk should be below MIN_SIZE but still exist";
  EXPECT_EQ(chunks.back().content, tiny_last) << "Last chunk should contain exactly the tiny paragraph";
}

// Test different blank line patterns
TEST_F(PlainTextExtractorTest, GetChunks_VariousBlankLinePatterns) {
  // Arrange - Test different ways of separating paragraphs
  size_t para_size = TestableExtractor::MIN_SIZE / 3;
  std::string para1 = create_paragraph(para_size, '1');
  std::string para2 = create_paragraph(para_size, '2');
  std::string para3 = create_paragraph(para_size, '3');
  std::string para4 = create_paragraph(para_size, '4');
  
  std::string content = para1 + "\n\n" +           // Single blank line
                       para2 + "\n\n\n" +         // Double blank line  
                       para3 + "\n   \n" +        // Blank line with spaces
                       para4;                      // No trailing separator

  auto file = create_test_file("blank_patterns.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Should properly split on all blank line patterns
  // Total content should be merged based on MIN_SIZE logic
  bool found_para1 = false;
  for (const auto& chunk : chunks) {
    if (chunk.content.find(std::string(10, '1')) != std::string::npos) {
      found_para1 = true;
      break;
    }
  }
  EXPECT_TRUE(found_para1) << "Should find content from first paragraph";
}

// Test content without paragraph breaks
TEST_F(PlainTextExtractorTest, GetChunks_NoParagraphBreaks_SingleChunk) {
  // Arrange - Text with line breaks but no blank lines (single paragraph)
  std::string content = "This is text content without paragraph breaks.\n";
  content += "It has line breaks but no blank lines.\n";
  content += "So it should be treated as a single paragraph.\n";
  content += create_content_of_size(TestableExtractor::MIN_SIZE / 2, "x"); // Pad to reasonable size

  auto file = create_test_file("no_breaks.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_EQ(chunks.size(), 1) << "Single paragraph should create single chunk";
  EXPECT_EQ(chunks[0].chunk_index, 0);
  EXPECT_EQ(chunks[0].content, content);
}

// Test with different line ending formats
TEST_F(PlainTextExtractorTest, GetChunks_WindowsLineEndings) {
  // Arrange - Use Windows line endings
  size_t para_size = TestableExtractor::MIN_SIZE / 2;
  std::string para1 = create_paragraph(para_size, 'w');
  std::string para2 = create_paragraph(para_size, 'i');
  std::string content = para1 + "\r\n\r\n" + para2;
  
  auto file = create_test_file("windows_endings.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Should handle Windows line endings properly
  for (const auto& chunk : chunks) {
    EXPECT_FALSE(chunk.content.empty());
  }
}

// Test with mixed line endings
TEST_F(PlainTextExtractorTest, GetChunks_MixedLineEndings) {
  // Arrange - Mix different line ending styles
  size_t para_size = TestableExtractor::MIN_SIZE / 3;
  std::string content = create_paragraph(para_size, 'u') + "\n\n";      // Unix
  content += create_paragraph(para_size, 'w') + "\r\n\r\n";            // Windows  
  content += create_paragraph(para_size, 'm');                         // End without separator
  
  auto file = create_test_file("mixed_endings.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Should handle mixed line endings
  for (const auto& chunk : chunks) {
    EXPECT_FALSE(chunk.content.empty());
  }
}

// Test error handling
TEST_F(PlainTextExtractorTest, GetChunks_NonExistentFile) {
  // Arrange
  auto non_existent = test_dir_ / "does_not_exist.txt";

  // Act & Assert
  EXPECT_THROW(extractor_->get_chunks(non_existent), std::runtime_error);
}

// Test with only whitespace
TEST_F(PlainTextExtractorTest, GetChunks_OnlyWhitespace) {
  // Arrange
  std::string content = "   \n\n\t\n   \n\n  ";
  auto file = create_test_file("whitespace.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0].content, content);
}

// Test exceeding max size triggers fixed chunking
TEST_F(PlainTextExtractorTest, GetChunks_ExceedsMaxSize_UsesFixedChunking) {
  // Arrange - Create paragraph that exceeds MAX_SIZE to trigger fixed chunking fallback
  size_t oversized_paragraph = TestableExtractor::MAX_SIZE + 500;
  std::string huge_paragraph = create_paragraph(oversized_paragraph, 'H');
  
  auto file = create_test_file("exceeds_max.txt", huge_paragraph);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 1) << "Oversized paragraph should be split using fixed chunking";
  
  // Most chunks should be close to FIXED_CHUNK_SIZE (except possibly the last)
  for (size_t i = 0; i < chunks.size() - 1; ++i) {
    EXPECT_LE(chunks[i].content.length(), TestableExtractor::FIXED_SIZE + 100) << "Chunk " << i << " should not greatly exceed FIXED_SIZE";
  }
}

// Test with special characters and unicode
TEST_F(PlainTextExtractorTest, GetChunks_SpecialCharacters) {
  // Arrange - Test with various special characters, sized appropriately
  std::string base_content = create_content_of_size(TestableExtractor::MIN_SIZE / 2, "x");
  std::string content = "Paragraph with special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?\n\n" + base_content + "\n\n";
  content += "Second paragraph with unicode: 🚀🌟🎉 你好世界 Привет мир\n\n";
  content += create_paragraph(TestableExtractor::MIN_SIZE / 4, 'u');

  auto file = create_test_file("special_chars.txt", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Special characters should be preserved
  bool found_special_chars = false;
  for (const auto& chunk : chunks) {
    if (chunk.content.find("!@#$%^&*()") != std::string::npos) {
      found_special_chars = true;
      break;
    }
  }
  EXPECT_TRUE(found_special_chars) << "Special characters should be preserved";
}

// Test performance and correctness with very large file
TEST_F(PlainTextExtractorTest, GetChunks_VeryLargeFile) {
  // Arrange - Create a very large file with multiple paragraphs of varying sizes
  std::string large_content;
  for (int i = 0; i < 10; ++i) {
    size_t para_size = TestableExtractor::MIN_SIZE + (i * 100); // Increasing paragraph sizes
    large_content += create_paragraph(para_size, 'a' + static_cast<char>(i)) + "\n\n";
  }
  auto file = create_test_file("very_large.txt", large_content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 1) << "Large file should be split into multiple chunks";
  
  // Verify all chunks have valid indices
  for (size_t i = 0; i < chunks.size(); ++i) {
    EXPECT_EQ(chunks[i].chunk_index, static_cast<int>(i));
    EXPECT_FALSE(chunks[i].content.empty());
  }
  
  // Total content should be approximately preserved 
  size_t total_chunk_length = 0;
  for (const auto& chunk : chunks) {
    total_chunk_length += chunk.content.length();
  }
  EXPECT_GE(total_chunk_length, large_content.length() * 0.9) << "Most content should be preserved";
}

// Test constants adaptation
TEST_F(PlainTextExtractorTest, AdaptsToChunkingConstants) {
  // This test verifies the test framework adapts to different chunking constants
  std::cout << "Current chunking constants for PlainTextExtractor:" << std::endl;
  std::cout << "MIN_CHUNK_SIZE: " << TestableExtractor::MIN_SIZE << std::endl;
  std::cout << "MAX_CHUNK_SIZE: " << TestableExtractor::MAX_SIZE << std::endl;
  std::cout << "FIXED_CHUNK_SIZE: " << TestableExtractor::FIXED_SIZE << std::endl;
  
  // Verify our test helpers create appropriately sized content
  std::string small_content = create_paragraph(TestableExtractor::MIN_SIZE / 2, 't');
  std::string min_content = create_paragraph(TestableExtractor::MIN_SIZE, 't');
  std::string large_content = create_paragraph(TestableExtractor::MAX_SIZE + 100, 't');
  
  EXPECT_LT(small_content.length(), TestableExtractor::MIN_SIZE);
  EXPECT_EQ(min_content.length(), TestableExtractor::MIN_SIZE);
  EXPECT_GT(large_content.length(), TestableExtractor::MAX_SIZE);
}

} // namespace magic_core