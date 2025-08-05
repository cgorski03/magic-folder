#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "magic_core/extractors/markdown_extractor.hpp"
#include "../../common/utilities_test.hpp"

namespace magic_core {

class MarkdownExtractorTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    extractor_ = std::make_unique<MarkdownExtractor>();
    
    // Create test directory
    test_dir_ = std::filesystem::temp_directory_path() / "markdown_tests";
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
  class TestableExtractor : public MarkdownExtractor {
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

  // Helper to create markdown sections of specific sizes
  std::string create_markdown_section(const std::string& title, size_t content_size, char fill_char = 'a') {
    std::string section = "# " + title + "\n\n";
    section += create_content_of_size(content_size, std::string(1, fill_char));
    return section;
  }

  std::string create_markdown_subsection(const std::string& title, size_t content_size, char fill_char = 'b') {
    std::string section = "## " + title + "\n\n";
    section += create_content_of_size(content_size, std::string(1, fill_char));
    return section;
  }

  std::unique_ptr<MarkdownExtractor> extractor_;
  std::filesystem::path test_dir_;
};

// Test can_handle method
TEST_F(MarkdownExtractorTest, CanHandle_MarkdownFiles) {
  EXPECT_TRUE(extractor_->can_handle("/path/to/file.md"));
  EXPECT_TRUE(extractor_->can_handle("/path/to/README.md"));
  EXPECT_TRUE(extractor_->can_handle("documentation.md"));
}

TEST_F(MarkdownExtractorTest, CanHandle_NonMarkdownFiles) {
  EXPECT_FALSE(extractor_->can_handle("/path/to/file.txt"));
  EXPECT_FALSE(extractor_->can_handle("/path/to/file.doc"));
  EXPECT_FALSE(extractor_->can_handle("/path/to/file.pdf"));
  EXPECT_FALSE(extractor_->can_handle("/path/to/file"));
  EXPECT_FALSE(extractor_->can_handle("file.MD")); // Case sensitive
}

// Test chunking with empty file
TEST_F(MarkdownExtractorTest, GetChunks_EmptyFile) {
  // Arrange
  auto empty_file = create_test_file("empty.md", "");

  // Act
  auto chunks = extractor_->get_chunks(empty_file);

  // Assert
  EXPECT_TRUE(chunks.empty());
}

// Test chunking where total content is below minimum chunk size
TEST_F(MarkdownExtractorTest, GetChunks_BelowMinimumSize_SingleChunk) {
  // Arrange - Total content under MIN_CHUNK_SIZE
  size_t small_content_size = TestableExtractor::MIN_SIZE / 2; // Half of minimum size
  std::string content = R"(# Title

Short intro.

## Section 1

Brief.
)";
  
  // Ensure our content is actually below minimum
  if (content.length() >= TestableExtractor::MIN_SIZE) {
    content = create_markdown_section("Small Title", small_content_size / 2, 'x');
  }
  
  auto file = create_test_file("below_min.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_EQ(chunks.size(), 1) << "All small content should merge into single chunk. MIN_SIZE=" << TestableExtractor::MIN_SIZE << ", content_size=" << content.length();
  EXPECT_EQ(chunks[0].content, content);
  EXPECT_EQ(chunks[0].chunk_index, 0);
}

TEST_F(MarkdownExtractorTest, GetChunks_ExactlyMinimumSize_SingleChunk) {
  // Arrange - Create content that's exactly MIN_CHUNK_SIZE
  std::string title = "# Exact Size Test\n\n";
  size_t content_size = TestableExtractor::MIN_SIZE - title.length();
  std::string content = title + create_content_of_size(content_size, "x");
  
  EXPECT_EQ(content.length(), TestableExtractor::MIN_SIZE) << "Content should be exactly MIN_SIZE";
  auto file = create_test_file("exact_min.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0].content, content);
}

TEST_F(MarkdownExtractorTest, GetChunks_SmallSections_MergingBehavior) {
  // Arrange - Multiple small sections that individually are below MIN_SIZE
  // but when merged together exceed MIN_SIZE
  size_t section_size = TestableExtractor::MIN_SIZE / 3 + 9; // Each section is 1/3 of minimum plus a little more so it will exceed MIN_SIZE
  
  std::string section1 = create_markdown_section("Section One", section_size, 'a');
  std::string section2 = create_markdown_subsection("Section Two", section_size, 'b'); 
  std::string section3 = create_markdown_subsection("Section Three", section_size / 2, 'c'); // Smaller last section
  
  std::string content = section1 + "\n\n" + section2 + "\n\n" + section3;
  std::cout << "Content: " << content << std::endl;
  auto file = create_test_file("merging.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);
  for (const auto& chunk : chunks) {
    std::cout << "Chunk: " << chunk.content << std::endl;
  }
  // Assert
  // First two sections should merge (together they exceed MIN_SIZE)
  // Last section should be separate due to "last section" rule
  EXPECT_EQ(chunks.size(), 2) << "Should merge small sections until MIN_SIZE is reached";
  
  EXPECT_GE(chunks[0].content.length(), TestableExtractor::MIN_SIZE) << "First chunk should be at least MIN_SIZE";
  // Last chunk might be below MIN_SIZE due to last section rule
  
  // Verify chunk indices are sequential
  for (size_t i = 0; i < chunks.size(); ++i) {
    EXPECT_EQ(chunks[i].chunk_index, static_cast<int>(i));
  }
}

TEST_F(MarkdownExtractorTest, GetChunks_LargeSections_SplittingBehavior) {
  // Arrange - Create a section that exceeds MAX_CHUNK_SIZE and needs splitting
  size_t large_section_size = TestableExtractor::MAX_SIZE + (TestableExtractor::MAX_SIZE / 2); // 1.5x max size
  std::string large_section = create_markdown_section("Large Section", large_section_size, 'L');
  
  std::string small_section = create_markdown_subsection("Small Section", TestableExtractor::MIN_SIZE / 2, 's');
  std::string content = large_section + "\n\n" + small_section;
  
  auto file = create_test_file("large_section.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 1) << "Large section should be split. MAX_SIZE=" << TestableExtractor::MAX_SIZE;
  
  // At least one chunk should contain part of the large content
  bool found_large_content = false;
  for (const auto& chunk : chunks) {
    if (chunk.content.find(std::string(50, 'L')) != std::string::npos) {
      found_large_content = true;
      break;
    }
  }
  EXPECT_TRUE(found_large_content) << "Should find large section content in chunks";
}

TEST_F(MarkdownExtractorTest, GetChunks_LastSectionBehavior) {
  // Arrange - Test the "last section" rule: even if last section is small, it gets its own chunk
  std::string large_section = create_markdown_section("Large Section", TestableExtractor::MIN_SIZE + 50, 'X');
  std::string tiny_last = "\n\n## Tiny Last\n\nSmall final section."; // Very small last section
  std::string content = large_section + tiny_last;
  
  auto file = create_test_file("last_section.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GE(chunks.size(), 2) << "Should have at least 2 chunks: large section + tiny last";
  
  // Last chunk should exist even though it's small
  EXPECT_LT(chunks.back().content.length(), TestableExtractor::MIN_SIZE) << "Last chunk should be below MIN_SIZE but still exist";
  EXPECT_TRUE(chunks.back().content.find("Tiny Last") != std::string::npos) << "Last chunk should contain the tiny section";
}

TEST_F(MarkdownExtractorTest, GetChunks_MultipleHeadingLevels) {
  // Arrange - Test different heading levels with content sized relative to MIN_SIZE
  size_t section_size = TestableExtractor::MIN_SIZE / 2;
  
  std::string content = create_markdown_section("Main Title", section_size, 'M') + "\n\n";
  content += create_markdown_subsection("Level 2 Heading", section_size, '2') + "\n\n";
  content += "### Level 3 Heading\n\n" + create_content_of_size(section_size, "3") + "\n\n";
  content += "#### Level 4 Heading\n\n" + create_content_of_size(section_size / 2, "4") + "\n\n";
  content += create_markdown_subsection("Another Level 2", section_size, 'A');

  auto file = create_test_file("multilevel.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Should split at each heading, but merge sections that are too small
  bool found_main_title = false;
  bool found_level2 = false;
  for (const auto& chunk : chunks) {
    if (chunk.content.find("# Main Title") != std::string::npos) {
      found_main_title = true;
    }
    if (chunk.content.find("## Level 2 Heading") != std::string::npos) {
      found_level2 = true;
    }
  }
  EXPECT_TRUE(found_main_title);
  EXPECT_TRUE(found_level2);
}

TEST_F(MarkdownExtractorTest, GetChunks_NoHeadings_SingleChunk) {
  // Arrange - Markdown content without headings, sized relative to MIN_SIZE
  std::string content = "This is markdown content without any headings.\n\n";
  content += "It has multiple paragraphs.\n\n";  
  content += "But no heading structure.\n\n";
  content += create_content_of_size(TestableExtractor::MIN_SIZE / 2, "t"); // Ensure total is reasonable size

  auto file = create_test_file("no_headings.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_EQ(chunks.size(), 1) << "Content without headings should be one chunk";
  EXPECT_EQ(chunks[0].chunk_index, 0);
  EXPECT_EQ(chunks[0].content, content);
}

TEST_F(MarkdownExtractorTest, GetChunks_WithCodeBlocks) {
  // Arrange - Test code blocks with content sized appropriately
  std::string intro_content = create_content_of_size(TestableExtractor::MIN_SIZE / 3, "i");
  std::string more_content = create_content_of_size(TestableExtractor::MIN_SIZE / 3, "m");
  
  std::string content = "# Code Example\n\n" + intro_content + "\n\n";
  content += "```cpp\n";
  content += "int main() {\n    return 0;\n}\n";
  content += "```\n\n";
  content += "## Another Section\n\n" + more_content + "\n\n";
  content += "More content with `inline code`.\n\n";
  content += "- List item 1\n- List item 2\n  - Nested item";

  auto file = create_test_file("with_code.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Verify code blocks are preserved
  bool found_code_block = false;
  for (const auto& chunk : chunks) {
    if (chunk.content.find("```cpp") != std::string::npos) {
      found_code_block = true;
      break;
    }
  }
  EXPECT_TRUE(found_code_block) << "Code blocks should be preserved in chunks";
}

TEST_F(MarkdownExtractorTest, GetChunks_ExceedsMaxSize_UsesFixedChunking) {
  // Arrange - Create content that exceeds MAX_CHUNK_SIZE to trigger fixed chunking fallback
  size_t oversized_content = TestableExtractor::MAX_SIZE + 500;
  std::string huge_section = create_markdown_section("Huge Section", oversized_content, 'H');
  
  auto file = create_test_file("exceeds_max.md", huge_section);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 1) << "Oversized content should be split using fixed chunking";
  
  // Most chunks should be close to FIXED_CHUNK_SIZE (except possibly the last)
  for (size_t i = 0; i < chunks.size() - 1; ++i) {
    EXPECT_LE(chunks[i].content.length(), TestableExtractor::FIXED_SIZE + 100) << "Chunk " << i << " should not greatly exceed FIXED_SIZE";
  }
}

// Test error handling
TEST_F(MarkdownExtractorTest, GetChunks_NonExistentFile) {
  // Arrange
  auto non_existent = test_dir_ / "does_not_exist.md";

  // Act & Assert
  EXPECT_THROW(extractor_->get_chunks(non_existent), ContentExtractorError);
}

// Test with different line ending formats
TEST_F(MarkdownExtractorTest, GetChunks_WindowsLineEndings) {
  // Arrange - Use Windows line endings
  size_t content_size = TestableExtractor::MIN_SIZE / 2;
  std::string content = "# Title\r\n\r\n";
  content += create_content_of_size(content_size, "w") + "\r\n\r\n";
  content += "## Section\r\n\r\n";
  content += create_content_of_size(content_size, "s");
  
  auto file = create_test_file("windows_endings.md", content);

  // Act
  auto chunks = extractor_->get_chunks(file);

  // Assert
  EXPECT_GT(chunks.size(), 0);
  
  // Should handle the different line endings properly
  for (const auto& chunk : chunks) {
    EXPECT_FALSE(chunk.content.empty());
  }
}

// Test constants adaptation
TEST_F(MarkdownExtractorTest, AdaptsToChunkingConstants) {
  // This test verifies the test framework adapts to different chunking constants
  std::cout << "Current chunking constants for MarkdownExtractor:" << std::endl;
  std::cout << "MIN_CHUNK_SIZE: " << TestableExtractor::MIN_SIZE << std::endl;
  std::cout << "MAX_CHUNK_SIZE: " << TestableExtractor::MAX_SIZE << std::endl;
  std::cout << "FIXED_CHUNK_SIZE: " << TestableExtractor::FIXED_SIZE << std::endl;
  
  // Verify our test helpers create appropriately sized content
  std::string small_content = create_content_of_size(TestableExtractor::MIN_SIZE / 2, "t");
  std::string min_content = create_content_of_size(TestableExtractor::MIN_SIZE, "t");
  std::string large_content = create_content_of_size(TestableExtractor::MAX_SIZE + 100, "t");
  
  EXPECT_LT(small_content.length(), TestableExtractor::MIN_SIZE);
  EXPECT_EQ(min_content.length(), TestableExtractor::MIN_SIZE);
  EXPECT_GT(large_content.length(), TestableExtractor::MAX_SIZE);
}

} // namespace magic_core 