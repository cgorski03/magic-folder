#pragma once
#include "content_extractor.hpp"

namespace magic_core {

class MarkdownExtractor : public ContentExtractor {
public:
    bool can_handle(const fs::path& file_path) const override;

    std::vector<Chunk> get_chunks(const fs::path& file_path) const override;
    
    ExtractionResult extract_with_hash(const fs::path& file_path) const override;

private:
    // Helper method to extract chunks from already-loaded content
    std::vector<Chunk> extract_chunks_from_content(const std::string& content) const;
};

}