#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace magic_core
{

  struct SearchResult
  {
    std::string path;
    float score;
  };

  class VectorStoreError : public std::exception
  {
  public:
    explicit VectorStoreError(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };

  class VectorStore
  {
  public:
    explicit VectorStore(const std::filesystem::path &db_path, const std::string &table_name);
    ~VectorStore();

    // Disable copy constructor and assignment
    VectorStore(const VectorStore &) = delete;
    VectorStore &operator=(const VectorStore &) = delete;

    // Allow move constructor and assignment
    VectorStore(VectorStore &&) noexcept;
    VectorStore &operator=(VectorStore &&) noexcept;

    // Add embedding for a file
    void add_embedding(const std::string &path, const std::vector<float> &vector);

    // Search for similar files
    std::vector<SearchResult> search_similar(const std::vector<float> &query_vector, size_t top_k);

    // Initialize the database
    void initialize();

  private:
    std::filesystem::path db_path_;
    std::string table_name_;

    // Database connection (placeholder - will be replaced with actual LanceDB or similar)
    struct DatabaseConnection;
    std::unique_ptr<DatabaseConnection> db_conn_;

    // Constants
    static constexpr size_t EMBEDDING_DIM = 1024;
    static constexpr const char *EMBEDDING_FIELD_NAME = "vector";

    // Helper methods
    void create_table_if_not_exists();
    void validate_vector_dimension(const std::vector<float> &vector);
  };

}