#pragma once

#include "magic_core/types/chunk.hpp"
#include <filesystem>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

namespace magic_core {

class ContentExtractorError : public std::exception {
public:
    explicit ContentExtractorError(const std::string& message) : message_(message) {}
    const char *what() const noexcept override { return message_.c_str(); }
private:
    std::string message_;
};
class ContentExtractor {
public:
    virtual ~ContentExtractor() = default;

    // Checks if this extractor can handle the given file extension
    virtual bool can_handle(const fs::path& file_path) const = 0;

    // opens, reads, and chunks the file
    virtual std::vector<Chunk> get_chunks(const fs::path& file_path) const = 0;

    std::string get_content_hash(const fs::path& file_path) const;
};

// Define a type for our smart pointers
using ContentExtractorPtr = std::unique_ptr<ContentExtractor>;

}