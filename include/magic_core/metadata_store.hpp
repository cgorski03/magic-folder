#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <chrono>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

namespace magic_core
{

  struct FileMetadata
  {
    int id;
    std::string path;
    std::string content_hash;
    std::chrono::system_clock::time_point last_modified;
    std::chrono::system_clock::time_point created_at;
    std::string file_type;
    size_t file_size;
  };

  class MetadataStoreError : public std::exception
  {
  public:
    explicit MetadataStoreError(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };

  class MetadataStore
  {
  public:
    explicit MetadataStore(const std::filesystem::path &db_path);
    ~MetadataStore();

    // Disable copy constructor and assignment
    MetadataStore(const MetadataStore &) = delete;
    MetadataStore &operator=(const MetadataStore &) = delete;

    // Allow move constructor and assignment
    MetadataStore(MetadataStore &&) noexcept;
    MetadataStore &operator=(MetadataStore &&) noexcept;

    // Initialize the database
    void initialize();

    // Add or update file metadata
    void upsert_file_metadata(const FileMetadata &metadata);

    // Get file metadata by path
    std::optional<FileMetadata> get_file_metadata(const std::string &path);

    // Get file metadata by ID
    std::optional<FileMetadata> get_file_metadata(int id);

    // Delete file metadata
    void delete_file_metadata(const std::string &path);

    // List all files
    std::vector<FileMetadata> list_all_files();

    // Check if file exists
    bool file_exists(const std::string &path);

  private:
    std::filesystem::path db_path_;
    sqlite3 *db_;

    // Helper methods
    void create_tables();
    void execute_sql(const std::string &sql);
    std::string compute_content_hash(const std::filesystem::path &file_path);
    std::chrono::system_clock::time_point get_file_last_modified(const std::filesystem::path &file_path);
  };

} // namespace magic_core