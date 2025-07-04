#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "magic_core/metadata_store.hpp"
#include "magic_core/types.hpp"
#include "magic_services/file_delete_service.hpp"
#include "test_utilities.hpp"

namespace magic_services {

class FileDeleteServiceTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    // Call parent setup to initialize metadata_store_
    MetadataStoreTestBase::SetUp();

    // Create the service with the metadata store
    file_delete_service_ = std::make_unique<FileDeleteService>(metadata_store_);

    // Set up test data using utilities
    setupTestData();
  }

  void setupTestData() {
    // Create basic test files using utilities
    test_files_.push_back(magic_tests::TestUtilities::create_test_file_metadata(
        "/test/file1.txt", "abc123", magic_core::FileType::Text, 1024, false));
    test_files_.push_back(magic_tests::TestUtilities::create_test_file_metadata(
        "/test/file2.md", "def456", magic_core::FileType::Markdown, 2048, false));
    test_files_.push_back(magic_tests::TestUtilities::create_test_file_metadata(
        "/test/file_with_vector.cpp", "vector123", magic_core::FileType::Code, 512, true));

    // Populate the metadata store
    magic_tests::TestUtilities::populate_metadata_store(metadata_store_, test_files_);
  }

  std::unique_ptr<FileDeleteService> file_delete_service_;
  std::vector<magic_core::FileMetadata> test_files_;
};

// Test successful file deletion
TEST_F(FileDeleteServiceTest, DeleteFile_RemovesFileFromDatabase) {
  // Arrange
  std::filesystem::path test_path = "/test/file1.txt";

  // Verify file exists before deletion
  ASSERT_TRUE(metadata_store_->file_exists(test_path));
  auto file_before = metadata_store_->get_file_metadata(test_path);
  ASSERT_TRUE(file_before.has_value());

  // Act
  file_delete_service_->delete_file(test_path);

  // Assert
  EXPECT_FALSE(metadata_store_->file_exists(test_path));
  auto file_after = metadata_store_->get_file_metadata(test_path);
  EXPECT_FALSE(file_after.has_value());
}

TEST_F(FileDeleteServiceTest, DeleteFile_OnlyRemovesSpecifiedFile) {
  // Arrange
  std::filesystem::path delete_path = "/test/file1.txt";
  std::filesystem::path keep_path = "/test/file2.md";

  // Verify both files exist before deletion
  ASSERT_TRUE(metadata_store_->file_exists(delete_path));
  ASSERT_TRUE(metadata_store_->file_exists(keep_path));

  // Act
  file_delete_service_->delete_file(delete_path);

  // Assert
  EXPECT_FALSE(metadata_store_->file_exists(delete_path));
  EXPECT_TRUE(metadata_store_->file_exists(keep_path));

  // Verify the kept file still has all its metadata
  auto kept_file = metadata_store_->get_file_metadata(keep_path);
  ASSERT_TRUE(kept_file.has_value());
  EXPECT_EQ(kept_file->path, "/test/file2.md");
  EXPECT_EQ(kept_file->content_hash, "def456");
  EXPECT_EQ(kept_file->file_type, magic_core::FileType::Markdown);
  EXPECT_EQ(kept_file->file_size, 2048);
}

TEST_F(FileDeleteServiceTest, DeleteFile_HandlesFileWithVectorEmbedding) {
  // Arrange
  std::filesystem::path test_path = "/test/file_with_vector.cpp";

  // Verify file exists and has vector embedding before deletion
  ASSERT_TRUE(metadata_store_->file_exists(test_path));
  auto file_before = metadata_store_->get_file_metadata(test_path);
  ASSERT_TRUE(file_before.has_value());
  ASSERT_EQ(file_before->vector_embedding.size(), 768);

  // Act
  file_delete_service_->delete_file(test_path);

  // Assert
  EXPECT_FALSE(metadata_store_->file_exists(test_path));
  auto file_after = metadata_store_->get_file_metadata(test_path);
  EXPECT_FALSE(file_after.has_value());
}

TEST_F(FileDeleteServiceTest, DeleteFile_HandlesNonExistentFile) {
  // Arrange
  std::filesystem::path nonexistent_path = "/test/nonexistent.txt";

  // Verify file doesn't exist
  ASSERT_FALSE(metadata_store_->file_exists(nonexistent_path));

  // Act & Assert - should not throw or crash
  EXPECT_NO_THROW(file_delete_service_->delete_file(nonexistent_path));

  // Verify other files are still intact
  EXPECT_TRUE(metadata_store_->file_exists("/test/file1.txt"));
  EXPECT_TRUE(metadata_store_->file_exists("/test/file2.md"));
  EXPECT_TRUE(metadata_store_->file_exists("/test/file_with_vector.cpp"));
}

TEST_F(FileDeleteServiceTest, DeleteFile_HandlesRelativePaths) {
  // Arrange - Add a file with a relative path
  auto relative_file = magic_tests::TestUtilities::create_test_file_metadata(
      "relative/path/file.txt", "rel123", magic_core::FileType::Text, 512, false);
  metadata_store_->upsert_file_metadata(relative_file);

  std::filesystem::path relative_path = "relative/path/file.txt";

  // Verify file exists before deletion
  ASSERT_TRUE(metadata_store_->file_exists(relative_path));

  // Act
  file_delete_service_->delete_file(relative_path);

  // Assert
  EXPECT_FALSE(metadata_store_->file_exists(relative_path));
}

TEST_F(FileDeleteServiceTest, DeleteFile_HandlesEmptyPath) {
  // Arrange
  std::filesystem::path empty_path = "";

  // Act & Assert - should not throw or crash
  EXPECT_NO_THROW(file_delete_service_->delete_file(empty_path));

  // Verify all existing files are still intact
  EXPECT_TRUE(metadata_store_->file_exists("/test/file1.txt"));
  EXPECT_TRUE(metadata_store_->file_exists("/test/file2.md"));
  EXPECT_TRUE(metadata_store_->file_exists("/test/file_with_vector.cpp"));
}

TEST_F(FileDeleteServiceTest, DeleteFile_UpdatesFileCount) {
  // Arrange
  auto initial_files = metadata_store_->list_all_files();
  size_t initial_count = initial_files.size();
  ASSERT_EQ(initial_count, 3);  // Should have 3 test files

  // Act
  file_delete_service_->delete_file("/test/file1.txt");

  // Assert
  auto remaining_files = metadata_store_->list_all_files();
  EXPECT_EQ(remaining_files.size(), initial_count - 1);

  // Verify the correct file was removed
  for (const auto& file : remaining_files) {
    EXPECT_NE(file.path, "/test/file1.txt");
  }
}

TEST_F(FileDeleteServiceTest, DeleteFile_MultipleConsecutiveDeletions) {
  // Arrange
  std::vector<std::string> paths_to_delete = {"/test/file1.txt", "/test/file2.md",
                                              "/test/file_with_vector.cpp"};

  // Act - Delete all files one by one
  for (const auto& path : paths_to_delete) {
    ASSERT_TRUE(metadata_store_->file_exists(path));  // Verify exists before deletion
    file_delete_service_->delete_file(path);
    EXPECT_FALSE(metadata_store_->file_exists(path));  // Verify deleted after deletion
  }

  // Assert - Database should be empty
  auto remaining_files = metadata_store_->list_all_files();
  EXPECT_TRUE(remaining_files.empty());
}

// Integration test to verify service properly delegates to metadata store
TEST_F(FileDeleteServiceTest, ServiceProperlyDelegatesToMetadataStore) {
  // Arrange
  std::filesystem::path test_path = "/test/file1.txt";

  // Act - Delete through service
  file_delete_service_->delete_file(test_path);

  // Assert - Verify the effect is the same as direct metadata store deletion
  EXPECT_FALSE(metadata_store_->file_exists(test_path));

  // Test that deleting the same file directly through metadata store
  // behaves the same way (doesn't crash)
  EXPECT_NO_THROW(metadata_store_->delete_file_metadata(test_path));
}

}  // namespace magic_services