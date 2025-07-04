#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "magic_core/metadata_store.hpp"
#include "magic_services/file_info_service.hpp"
#include "test_utilities.hpp"

namespace magic_services {

class FileInfoServiceTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    // Call parent setup to initialize metadata_store_
    MetadataStoreTestBase::SetUp();

    // Create the service with the metadata store
    file_info_service_ = std::make_unique<FileInfoService>(metadata_store_);

    // Set up test data using utilities
    setupTestData();
  }

  void setupTestData() {
    // Create basic test files using utilities
    test_files_.push_back(magic_tests::TestUtilities::create_test_file_metadata(
        "/test/file1.txt", "abc123", magic_core::FileType::Text, 1024, false));
    test_files_.push_back(magic_tests::TestUtilities::create_test_file_metadata(
        "/test/file2.md", "def456", magic_core::FileType::Markdown, 2048, false));

    // Populate the metadata store
    magic_tests::TestUtilities::populate_metadata_store(metadata_store_, test_files_);
  }

  std::unique_ptr<FileInfoService> file_info_service_;
  std::vector<magic_core::FileMetadata> test_files_;
};

// Test list_files() method
TEST_F(FileInfoServiceTest, ListFiles_ReturnsAllFiles) {
  // Act
  auto result = file_info_service_->list_files();

  // Assert
  ASSERT_EQ(result.size(), 2);

  // Sort results by path for consistent testing
  std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) { return a.path < b.path; });

  EXPECT_EQ(result[0].path, "/test/file1.txt");
  EXPECT_EQ(result[0].content_hash, "abc123");
  EXPECT_EQ(result[0].file_type, magic_core::FileType::Text);
  EXPECT_EQ(result[0].file_size, 1024);

  EXPECT_EQ(result[1].path, "/test/file2.md");
  EXPECT_EQ(result[1].content_hash, "def456");
  EXPECT_EQ(result[1].file_type, magic_core::FileType::Markdown);
  EXPECT_EQ(result[1].file_size, 2048);
}

TEST_F(FileInfoServiceTest, ListFiles_ReturnsEmptyWhenNoFiles) {
  // Arrange - clear all test data
  metadata_store_->delete_file_metadata("/test/file1.txt");
  metadata_store_->delete_file_metadata("/test/file2.md");

  // Act
  auto result = file_info_service_->list_files();

  // Assert
  EXPECT_TRUE(result.empty());
}

TEST_F(FileInfoServiceTest, ListFiles_PreservesVectorEmbeddings) {
  // Arrange - Create a file with vector embedding using utilities
  auto file_with_vector = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/file_with_vector.txt", "vector123", magic_core::FileType::Text, 512, true);

  metadata_store_->upsert_file_metadata(file_with_vector);

  // Act
  auto result = file_info_service_->list_files();

  // Assert - should have original 2 + 1 with vector = 3 total
  ASSERT_EQ(result.size(), 3);

  // Find the file with vector in results
  auto vector_file_it = std::find_if(result.begin(), result.end(), [](const auto& f) {
    return f.path == "/test/file_with_vector.txt";
  });
  ASSERT_NE(vector_file_it, result.end());

  EXPECT_EQ(vector_file_it->vector_embedding.size(), 768);
  // Check that vector is properly preserved (non-zero values)
  EXPECT_GT(vector_file_it->vector_embedding[0], 0.0f);
  EXPECT_GT(vector_file_it->vector_embedding[100], 0.0f);
}

// Test get_file_info() method
TEST_F(FileInfoServiceTest, GetFileInfo_ReturnsFileWhenExists) {
  // Arrange
  std::filesystem::path test_path = "/test/file1.txt";

  // Act
  auto result = file_info_service_->get_file_info(test_path);

  // Assert
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->path, "/test/file1.txt");
  EXPECT_EQ(result->content_hash, "abc123");
  EXPECT_EQ(result->file_type, magic_core::FileType::Text);
  EXPECT_EQ(result->file_size, 1024);
}

TEST_F(FileInfoServiceTest, GetFileInfo_ReturnsNulloptWhenFileNotExists) {
  // Arrange
  std::filesystem::path test_path = "/test/nonexistent.txt";

  // Act
  auto result = file_info_service_->get_file_info(test_path);

  // Assert
  EXPECT_FALSE(result.has_value());
}

TEST_F(FileInfoServiceTest, GetFileInfo_HandlesRelativePaths) {
  // Arrange - Add a file with a relative path using utilities
  auto relative_file = magic_tests::TestUtilities::create_test_file_metadata(
      "relative/path/file.txt", "rel123", magic_core::FileType::Text, 512, false);

  metadata_store_->upsert_file_metadata(relative_file);

  std::filesystem::path test_path = "relative/path/file.txt";

  // Act
  auto result = file_info_service_->get_file_info(test_path);

  // Assert
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->path, "relative/path/file.txt");
  EXPECT_EQ(result->content_hash, "rel123");
}

TEST_F(FileInfoServiceTest, GetFileInfo_PreservesTimestamps) {
  // Arrange
  std::filesystem::path test_path = "/test/file1.txt";

  // Act
  auto result = file_info_service_->get_file_info(test_path);

  // Assert
  ASSERT_TRUE(result.has_value());

  // Check that timestamps are preserved (within a reasonable tolerance)
  auto original_file = std::find_if(test_files_.begin(), test_files_.end(),
                                    [](const auto& f) { return f.path == "/test/file1.txt"; });
  ASSERT_NE(original_file, test_files_.end());

  // Note: Due to time precision differences in storage/retrieval, we check
  // that timestamps are approximately equal (within 1 second)
  auto time_diff_modified = std::chrono::duration_cast<std::chrono::seconds>(
                                result->last_modified - original_file->last_modified)
                                .count();
  auto time_diff_created = std::chrono::duration_cast<std::chrono::seconds>(
                               result->created_at - original_file->created_at)
                               .count();

  EXPECT_LE(std::abs(time_diff_modified), 1);
  EXPECT_LE(std::abs(time_diff_created), 1);
}

// Test edge cases and error conditions
TEST_F(FileInfoServiceTest, GetFileInfo_HandlesEmptyPath) {
  // Arrange
  std::filesystem::path empty_path = "";

  // Act
  auto result = file_info_service_->get_file_info(empty_path);

  // Assert
  EXPECT_FALSE(result.has_value());
}

// Integration-style tests (testing that the service properly delegates to the store)
TEST_F(FileInfoServiceTest, ServiceProperlyDelegatesToMetadataStore) {
  // This test verifies that the service correctly delegates to the metadata store
  // by checking that operations through the service have the same effect as
  // operations directly on the store

  // Test list_files delegation
  auto service_files = file_info_service_->list_files();
  auto store_files = metadata_store_->list_all_files();

  EXPECT_EQ(service_files.size(), store_files.size());

  // Test get_file_info delegation
  auto service_file = file_info_service_->get_file_info("/test/file1.txt");
  auto store_file = metadata_store_->get_file_metadata("/test/file1.txt");

  ASSERT_TRUE(service_file.has_value());
  ASSERT_TRUE(store_file.has_value());
  EXPECT_EQ(service_file->path, store_file->path);
  EXPECT_EQ(service_file->content_hash, store_file->content_hash);
}

// Test with larger datasets to ensure performance is reasonable
TEST_F(FileInfoServiceTest, ListFiles_HandlesLargeDataset) {
  // Arrange - add many more files using utilities
  auto large_dataset =
      magic_tests::TestUtilities::create_test_dataset(100, "/test/large_dataset", false);
  magic_tests::TestUtilities::populate_metadata_store(metadata_store_, large_dataset);

  // Act
  auto result = file_info_service_->list_files();

  // Assert - should have original 2 + 100 new files = 102 total
  EXPECT_EQ(result.size(), 102);

  // Verify that all files are properly returned
  std::set<std::string> paths;
  for (const auto& file : result) {
    paths.insert(file.path);
  }

  EXPECT_EQ(paths.size(), 102);
  EXPECT_NE(paths.find("/test/file1.txt"), paths.end());
  EXPECT_NE(paths.find("/test/file2.md"), paths.end());
  EXPECT_NE(paths.find("/test/large_dataset/file0.txt"), paths.end());
  EXPECT_NE(paths.find("/test/large_dataset/file99.txt"), paths.end());
}

// Test file update scenarios
TEST_F(FileInfoServiceTest, GetFileInfo_ReflectsUpdates) {
  // Arrange - update an existing file using utilities
  auto updated_file = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/file1.txt", "updated_hash", magic_core::FileType::Text, 2048, false);

  metadata_store_->upsert_file_metadata(updated_file);

  // Act
  auto result = file_info_service_->get_file_info("/test/file1.txt");

  // Assert
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->content_hash, "updated_hash");
  EXPECT_EQ(result->file_size, 2048);
}

// Test vector embedding retrieval specifically
TEST_F(FileInfoServiceTest, GetFileInfo_PreservesVectorEmbedding) {
  // Arrange - Create a file with vector embedding using utilities
  auto file_with_vector = magic_tests::TestUtilities::create_test_file_metadata(
      "/test/vector_file.txt", "vector456", magic_core::FileType::Text, 1024, true);

  metadata_store_->upsert_file_metadata(file_with_vector);

  // Act
  auto result = file_info_service_->get_file_info("/test/vector_file.txt");

  // Assert
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->vector_embedding.size(), 768);

  // Check that vector values are reasonable (deterministic based on path)
  EXPECT_GT(result->vector_embedding[0], 0.0f);
  EXPECT_LT(result->vector_embedding[0], 1.0f);
  EXPECT_GT(result->vector_embedding[100], 0.0f);
  EXPECT_LT(result->vector_embedding[100], 1.0f);
}

}  // namespace magic_services