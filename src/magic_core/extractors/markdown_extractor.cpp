#include "magic_core/extractors/markdown_extractor.hpp"


bool MarkdownExtractor::can_handle(const std::filesystem::path& file_path) const {
    return file_path.extension() == ".md";
}

std::vector<Chunk> MarkdownExtractor::get_chunks(const std::filesystem::path& file_path) const {
    // TODO
    return {};
}