#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "magic_core/db/database_manager.hpp"
#include "magic_core/db/metadata_store.hpp"
#include "magic_core/db/task_queue_repo.hpp"

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
  static magic_core::BasicFileMetadata create_test_basic_file_metadata(
      const std::string& path,
      const std::string& content_hash = "default_hash",
      magic_core::FileType file_type = magic_core::FileType::Text,
      size_t file_size = 1024,
      magic_core::ProcessingStatus processing_status = magic_core::ProcessingStatus::PROCESSED,
      const std::string& original_path = "",
      const std::string& tags = "");

  static magic_core::FileMetadata create_test_file_metadata(
      const std::string& path,
      const std::string& content_hash = "default_hash",
      magic_core::FileType file_type = magic_core::FileType::Text,
      size_t file_size = 1024,
      bool include_vector = false,
      magic_core::ProcessingStatus processing_status = magic_core::ProcessingStatus::PROCESSED,
      const std::string& original_path = "",
      const std::string& tags = "",
      const std::string& suggested_category = "",
      const std::string& suggested_filename = "");

  static std::vector<magic_core::FileMetadata> create_test_dataset(
      int count, const std::string& prefix = "/test", bool include_vectors = false);

  static std::vector<float> create_test_vector(const std::string& seed_text, int dimension = 1024);

  static Chunk create_test_chunk_with_embedding(
      const std::string& content, int chunk_index, const std::string& seed_text);

  static std::vector<Chunk> create_test_chunks(
      int count, const std::string& base_content = "test content");

  // Metadata store utilities - updated for new API
  static void populate_metadata_store_with_stubs(
      std::shared_ptr<magic_core::MetadataStore> store,
      const std::vector<magic_core::BasicFileMetadata>& files);

  static int create_complete_file_in_store(
      std::shared_ptr<magic_core::MetadataStore> store,
      const magic_core::FileMetadata& metadata,
      const std::vector<Chunk>& chunks = {});
};

/**
 * Base test fixture that provides common setup for tests requiring a MetadataStore
 */
class MetadataStoreTestBase : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_db_path_ = TestUtilities::create_temp_test_db();
    db_manager_ = std::make_shared<magic_core::DatabaseManager>(temp_db_path_);
    metadata_store_ = std::make_shared<magic_core::MetadataStore>(*db_manager_);
    task_queue_repo_ = std::make_shared<magic_core::TaskQueueRepo>(*db_manager_);
  }

  void TearDown() override {
    metadata_store_.reset();
    TestUtilities::cleanup_temp_db(temp_db_path_);
  }

  std::filesystem::path temp_db_path_;
  std::shared_ptr<magic_core::DatabaseManager> db_manager_;
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::shared_ptr<magic_core::TaskQueueRepo> task_queue_repo_;
};

}  // namespace magic_tests