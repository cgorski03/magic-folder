#pragma once

#include "magic_core/types/chunk.hpp"
#include <filesystem>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

class ContentExtractor {
public:
    virtual ~ContentExtractor() = default;

    // Checks if this extractor can handle the given file extension
    virtual bool can_handle(const fs::path& file_path) const = 0;

    // opens, reads, and chunks the file
    virtual std::vector<Chunk> get_chunks(const fs::path& file_path) const = 0;
};

// Define a type for our smart pointers
using ContentExtractorPtr = std::unique_ptr<ContentExtractor>;