#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>

namespace magic_core
{

  enum class FileType
  {
    Text,
    PDF,
    Markdown,
    Code,
    Unknown
  };

  struct ExtractedContent
  {
    std::string text_content;
    std::string title;
    std::vector<std::string> keywords;
    FileType file_type;
    size_t word_count;
    std::string language; // For code files
  };

  class ContentExtractorError : public std::exception
  {
  public:
    explicit ContentExtractorError(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };

  class ContentExtractor
  {
  public:
    ContentExtractor();
    ~ContentExtractor();

    // Disable copy constructor and assignment
    ContentExtractor(const ContentExtractor &) = delete;
    ContentExtractor &operator=(const ContentExtractor &) = delete;

    // Allow move constructor and assignment
    ContentExtractor(ContentExtractor &&) noexcept;
    ContentExtractor &operator=(ContentExtractor &&) noexcept;

    // Extract content from file
    ExtractedContent extract_content(const std::filesystem::path &file_path);

    // Extract content from text
    ExtractedContent extract_from_text(const std::string &text, const std::string &filename = "");

    // Detect file type
    FileType detect_file_type(const std::filesystem::path &file_path);

    // Check if file type is supported
    bool is_supported_file_type(const std::filesystem::path &file_path);

    // Get supported file extensions
    std::vector<std::string> get_supported_extensions() const;

  private:
    // File type handlers
    ExtractedContent extract_text_file(const std::filesystem::path &file_path);
    ExtractedContent extract_pdf_file(const std::filesystem::path &file_path);
    ExtractedContent extract_markdown_file(const std::filesystem::path &file_path);
    ExtractedContent extract_code_file(const std::filesystem::path &file_path);

    // Helper methods
    std::string read_file_content(const std::filesystem::path &file_path);
    std::string extract_title_from_content(const std::string &content);
    std::vector<std::string> extract_keywords(const std::string &content);
    std::string detect_language(const std::filesystem::path &file_path);
    size_t count_words(const std::string &text);

    // Supported file extensions
    std::vector<std::string> supported_extensions_;
  };

}