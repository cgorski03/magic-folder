#pragma once

#include "content_extractor.hpp"

namespace magic_core {

class PlainTextExtractor : public ContentExtractor {
public:
    bool can_handle(const fs::path& file_path) const override;

    std::vector<Chunk> get_chunks(const fs::path& file_path) const override;
};

}