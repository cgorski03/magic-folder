#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "magic_core/types/chunk.hpp"
#include "magic_core/types/file.hpp"

namespace fs = std::filesystem;

namespace magic_core {

class ContentExtractorError : public std::exception {
 public:
  explicit ContentExtractorError(const std::string& message) : message_(message) {}
  const char* what() const noexcept override {
    return message_.c_str();
  }

 private:
  std::string message_;
};

// Add this struct after the ContentExtractorError class
struct ExtractionResult {
  std::string content_hash;
  std::vector<Chunk> chunks;
};

class ContentExtractor {
 public:
  virtual ~ContentExtractor() = default;

  // Checks if this extractor can handle the given file extension
  virtual bool can_handle(const fs::path& file_path) const = 0;

  // opens, reads, and chunks the file
  virtual std::vector<Chunk> get_chunks(const fs::path& file_path) const = 0;

  // Combined operation - gets both hash and chunks in single file read
  virtual ExtractionResult extract_with_hash(const fs::path& file_path) const = 0;

  std::string get_content_hash(const fs::path& file_path) const;

  virtual FileType get_file_type() const;
  protected:
  // Helper method for derived classes to compute hash from loaded content
  std::string get_string_content(const fs::path& file_path) const;
  std::string compute_hash_from_content(const std::string& content) const;
  
  // --- Token-based goals ---
  static constexpr size_t TARGET_MAX_TOKENS = 512; 
  static constexpr size_t TARGET_MIN_TOKENS = 32;
  static constexpr size_t TARGET_FIXED_TOKENS = 384;
  static constexpr size_t TARGET_OVERLAP_TOKENS = 50;

  // --- Heuristic for conversion ---
  static constexpr float CHAR_PER_TOKEN_ESTIMATE = 3.5f;

  static constexpr size_t MAX_CHUNK_SIZE =
      static_cast<size_t>(TARGET_MAX_TOKENS * CHAR_PER_TOKEN_ESTIMATE);
  static constexpr size_t MIN_CHUNK_SIZE =
      static_cast<size_t>(TARGET_MIN_TOKENS * CHAR_PER_TOKEN_ESTIMATE);
  static constexpr size_t FIXED_CHUNK_SIZE =
      static_cast<size_t>(TARGET_FIXED_TOKENS * CHAR_PER_TOKEN_ESTIMATE);
  static constexpr size_t OVERLAP_SIZE =
      static_cast<size_t>(TARGET_OVERLAP_TOKENS * CHAR_PER_TOKEN_ESTIMATE);

  std::vector<std::string> split_into_fixed_chunks(const std::string& text) const;
};

// Define a type for our smart pointers
using ContentExtractorPtr = std::unique_ptr<ContentExtractor>;

}  // namespace magic_core