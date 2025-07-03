#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "magic_core/metadata_store.hpp"

namespace magic_tests {

/**
 * Utility class providing common functionality for all tests
 */
class TestUtilities {
 public:
  // Database utilities
  static std::filesystem::path create_temp_test_db();
  static void cleanup_temp_db(const std::filesystem::path& db_path);

  // Test data creation
  static magic_core::FileMetadata create_test_file_metadata(
      const std::string& path,
      const std::string& content_hash = "default_hash",
      const std::string& file_type = "text/plain",
      size_t file_size = 1024,
      bool include_vector = false);

  static std::vector<magic_core::FileMetadata> create_test_dataset(
      int count, const std::string& prefix = "/test", bool include_vectors = false);

  // Metadata store utilities
  static void populate_metadata_store(std::shared_ptr<magic_core::MetadataStore> store,
                                      const std::vector<magic_core::FileMetadata>& files);
};

/**
 * Base test fixture that provides common setup for tests requiring a MetadataStore
 */
class MetadataStoreTestBase : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_db_path_ = TestUtilities::create_temp_test_db();
    metadata_store_ = std::make_shared<magic_core::MetadataStore>(temp_db_path_);
  }

  void TearDown() override {
    metadata_store_.reset();
    TestUtilities::cleanup_temp_db(temp_db_path_);
  }

  std::filesystem::path temp_db_path_;
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
};

}  // namespace magic_tests