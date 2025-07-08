#pragma once
#include "content_extractor.hpp"

class MarkdownExtractor : public ContentExtractor {
public:
    bool can_handle(const fs::path& file_path) const override;

    std::vector<Chunk> get_chunks(const fs::path& file_path) const override;
};