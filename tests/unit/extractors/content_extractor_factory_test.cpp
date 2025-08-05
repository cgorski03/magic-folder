#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "magic_core/extractors/content_extractor_factory.hpp"
#include "magic_core/extractors/markdown_extractor.hpp"
#include "magic_core/extractors/plaintext_extractor.hpp"
#include "../../common/utilities_test.hpp"

namespace magic_core {

class ContentExtractorFactoryTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    factory_ = std::make_unique<ContentExtractorFactory>();
    
    // Create test directory
    test_dir_ = std::filesystem::temp_directory_path() / "factory_tests";
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

  std::unique_ptr<ContentExtractorFactory> factory_;
  std::filesystem::path test_dir_;
};

// Test factory for markdown files
TEST_F(ContentExtractorFactoryTest, GetExtractor_MarkdownFiles) {
  // Test different markdown file extensions
  std::vector<std::string> markdown_files = {
    "test.md",
    "README.md", 
    "documentation.md",
    "notes.md"
  };

  for (const auto& filename : markdown_files) {
    auto file_path = test_dir_ / filename;
    
    // Act & Assert - should not throw
    EXPECT_NO_THROW({
      auto& extractor = factory_->get_extractor_for(file_path);
      
      // Verify it's a MarkdownExtractor using dynamic_cast  
      const MarkdownExtractor* markdown_extractor = dynamic_cast<const MarkdownExtractor*>(&extractor);
      EXPECT_NE(markdown_extractor, nullptr) << "Should be MarkdownExtractor for " << filename;
      
      // Verify it can handle the file type
      EXPECT_TRUE(extractor.can_handle(file_path)) << "Extractor should handle " << filename;
    }) << "Should create extractor for " << filename;
  }
}

// Test factory for text files
TEST_F(ContentExtractorFactoryTest, GetExtractor_TextFiles) {
  // Test different text file extensions
  std::vector<std::string> text_files = {
    "document.txt",
    "log.txt", 
    "readme.txt",
    "notes.txt"
  };

  for (const auto& filename : text_files) {
    auto file_path = test_dir_ / filename;
    
    // Act & Assert - should not throw
    EXPECT_NO_THROW({
      auto& extractor = factory_->get_extractor_for(file_path);
      
      // Verify it's a PlainTextExtractor
      const PlainTextExtractor* text_extractor = dynamic_cast<const PlainTextExtractor*>(&extractor);
      EXPECT_NE(text_extractor, nullptr) << "Should be PlainTextExtractor for " << filename;
      
      // Verify it can handle the file type
      EXPECT_TRUE(extractor.can_handle(file_path)) << "Extractor should handle " << filename;
    }) << "Should create extractor for " << filename;
  }
}

// Test factory for unsupported files
TEST_F(ContentExtractorFactoryTest, GetExtractor_UnsupportedFiles) {
  // Test various unsupported file extensions - should throw exceptions
  std::vector<std::string> unsupported_files = {
    "document.docx",
    "presentation.pptx", 
    "spreadsheet.xlsx", 
    "image.jpg",
    "video.mp4",
    "archive.zip",
    "binary.exe",
    "config",           // No extension
    "makefile",         // No extension but not supported
    "file.unknown"      // Unknown extension
  };

  for (const auto& filename : unsupported_files) {
    auto file_path = test_dir_ / filename;
    
         // Should throw std::runtime_error for unsupported files
     EXPECT_THROW({
       auto& extractor = factory_->get_extractor_for(file_path);
     }, std::runtime_error) << "Should throw exception for " << filename;
  }
}

// Test case sensitivity of file extensions
TEST_F(ContentExtractorFactoryTest, GetExtractor_CaseSensitive) {
  // Test that extensions are case-sensitive (expecting lowercase)
  std::vector<std::pair<std::string, bool>> case_tests = {
    {"test.md", true},      // Lowercase - should work
    {"test.MD", false},     // Uppercase - should not work
    {"test.Md", false},     // Mixed case - should not work
    {"test.txt", true},     // Lowercase - should work
    {"test.TXT", false},    // Uppercase - should not work
    {"test.Txt", false}     // Mixed case - should not work
  };

  for (const auto& [filename, should_work] : case_tests) {
    auto file_path = test_dir_ / filename;
    
    if (should_work) {
             EXPECT_NO_THROW({
         auto& extractor = factory_->get_extractor_for(file_path);
       }) << "Should create extractor for " << filename;
     } else {
       EXPECT_THROW({
         auto& extractor = factory_->get_extractor_for(file_path);
       }, std::runtime_error) << "Should throw for " << filename;
    }
  }
}

// Test with actual file content (integration test)
TEST_F(ContentExtractorFactoryTest, GetExtractor_WithFileContent_Markdown) {
  // Arrange - Create actual markdown file
  std::string markdown_content = R"(# Test Document

This is a test markdown document with:

- List items
- Multiple paragraphs

## Section 2

More content here.

```cpp
int main() { return 0; }
```
)";

  auto md_file = create_test_file("test_content.md", markdown_content);

     // Act & Assert - should not throw
   ASSERT_NO_THROW({
     auto& extractor = factory_->get_extractor_for(md_file);
  
    // Verify it can extract chunks
    auto chunks = extractor.get_chunks(md_file);
    EXPECT_GT(chunks.size(), 0) << "Should extract at least one chunk";
    
    // Verify chunks contain expected content
    bool found_main_heading = false;
    bool found_code_block = false;
    
    for (const auto& chunk : chunks) {
      if (chunk.content.find("# Test Document") != std::string::npos) {
        found_main_heading = true;
      }
      if (chunk.content.find("```cpp") != std::string::npos) {
        found_code_block = true;
      }
    }
    
    EXPECT_TRUE(found_main_heading) << "Should find main heading in chunks";
    EXPECT_TRUE(found_code_block) << "Should find code block in chunks";
  });
}

// Test with actual file content (integration test)
TEST_F(ContentExtractorFactoryTest, GetExtractor_WithFileContent_PlainText) {
  // Arrange - Create actual text file
  std::string text_content = R"(This is a plain text document.

It has multiple paragraphs separated by blank lines.

This is the third paragraph with more content to test the chunking behavior.

Final paragraph for testing purposes.
)";

  auto txt_file = create_test_file("test_content.txt", text_content);

     // Act & Assert - should not throw
   ASSERT_NO_THROW({
     auto& extractor = factory_->get_extractor_for(txt_file);
  
    // Verify it can extract chunks
    auto chunks = extractor.get_chunks(txt_file);
    EXPECT_GT(chunks.size(), 0) << "Should extract at least one chunk";
    
    // Verify chunks contain expected content
    bool found_first_paragraph = false;
    bool found_final_paragraph = false;
    
    for (const auto& chunk : chunks) {
      if (chunk.content.find("This is a plain text document") != std::string::npos) {
        found_first_paragraph = true;
      }
      if (chunk.content.find("Final paragraph") != std::string::npos) {
        found_final_paragraph = true;
      }
    }
    
    EXPECT_TRUE(found_first_paragraph) << "Should find first paragraph in chunks";
    EXPECT_TRUE(found_final_paragraph) << "Should find final paragraph in chunks";
  });
}

// Test factory with non-existent files
TEST_F(ContentExtractorFactoryTest, GetExtractor_NonExistentFile) {
  // Test that factory works even for non-existent files (based on extension only)
  auto non_existent_md = test_dir_ / "does_not_exist.md";
  auto non_existent_txt = test_dir_ / "missing.txt";
  auto non_existent_unknown = test_dir_ / "missing.unknown";

     // Should work for known extensions even if file doesn't exist
   EXPECT_NO_THROW({
     auto& md_extractor = factory_->get_extractor_for(non_existent_md);
   }) << "Should create extractor for .md even if file doesn't exist";

   EXPECT_NO_THROW({
     auto& txt_extractor = factory_->get_extractor_for(non_existent_txt);
   }) << "Should create extractor for .txt even if file doesn't exist";

   // Should throw for unknown extensions
   EXPECT_THROW({
     auto& unknown_extractor = factory_->get_extractor_for(non_existent_unknown);
   }, std::runtime_error) << "Should throw for unknown extension";
}

// Test paths with special characters
TEST_F(ContentExtractorFactoryTest, GetExtractor_SpecialCharacterPaths) {
  // Test various path formats and special characters
  std::vector<std::pair<std::string, bool>> special_paths = {
    {"test file with spaces.md", true},
    {"test-with-dashes.txt", true},
    {"test_with_underscores.md", true},
    {"test.file.with.dots.txt", true},
    {"тест.md", true},                    // Unicode filename
    {"123numeric.txt", true},             // Starts with numbers
    {".hidden.md", true},                 // Hidden file
    {"file.md.backup", false},            // Double extension - should take last one
    {"noextension", false}                // No extension
  };

  for (const auto& [filename, should_work] : special_paths) {
    auto file_path = test_dir_ / filename;
    
    if (should_work) {
             EXPECT_NO_THROW({
         auto& extractor = factory_->get_extractor_for(file_path);
       }) << "Should create extractor for '" << filename << "'";
     } else {
       EXPECT_THROW({
         auto& extractor = factory_->get_extractor_for(file_path);
       }, std::runtime_error) << "Should throw for '" << filename << "'";
    }
  }
}

// Test factory consistency - same input should give same result
TEST_F(ContentExtractorFactoryTest, GetExtractor_Consistency) {
  auto file_path = test_dir_ / "consistency_test.md";
  
     // Get extractors multiple times - all should work and be the same type
   EXPECT_NO_THROW({
     auto& extractor1 = factory_->get_extractor_for(file_path);
     auto& extractor2 = factory_->get_extractor_for(file_path);
     auto& extractor3 = factory_->get_extractor_for(file_path);

    // All should be the same type (MarkdownExtractor)
    EXPECT_NE(dynamic_cast<const MarkdownExtractor*>(&extractor1), nullptr);
    EXPECT_NE(dynamic_cast<const MarkdownExtractor*>(&extractor2), nullptr);
    EXPECT_NE(dynamic_cast<const MarkdownExtractor*>(&extractor3), nullptr);

    // All should be able to handle the file
    EXPECT_TRUE(extractor1.can_handle(file_path));
    EXPECT_TRUE(extractor2.can_handle(file_path));
    EXPECT_TRUE(extractor3.can_handle(file_path));
  });
}

// Test edge cases
TEST_F(ContentExtractorFactoryTest, GetExtractor_EdgeCases) {
     // Test empty paths - should throw
   std::filesystem::path empty_path;
   EXPECT_THROW({
     auto& empty_extractor = factory_->get_extractor_for(empty_path);
   }, std::runtime_error) << "Should throw for empty path";

  // Test paths with only extension
  auto dot_md = test_dir_ / ".md";
  auto dot_txt = test_dir_ / ".txt";
  
  // These might be valid depending on implementation - just ensure they don't crash
  // The behavior can vary, so we just test that it doesn't throw exceptions
     EXPECT_NO_THROW({
     try {
       auto& result1 = factory_->get_extractor_for(dot_md);
       auto& result2 = factory_->get_extractor_for(dot_txt);
     } catch (const std::runtime_error&) {
       // Either outcome (success or exception) is acceptable for edge cases
     }
   });
}

// Test multiple factory instances
TEST_F(ContentExtractorFactoryTest, MultipleFactoryInstances) {
  auto factory2 = std::make_unique<ContentExtractorFactory>();
  auto file_path = test_dir_ / "multi_factory_test.md";

     // Both factories should work for the same file type
   EXPECT_NO_THROW({
     auto& extractor1 = factory_->get_extractor_for(file_path);
     auto& extractor2 = factory2->get_extractor_for(file_path);

    // Both should be markdown extractors
    EXPECT_NE(dynamic_cast<const MarkdownExtractor*>(&extractor1), nullptr);
    EXPECT_NE(dynamic_cast<const MarkdownExtractor*>(&extractor2), nullptr);
  });
}

} // namespace magic_core