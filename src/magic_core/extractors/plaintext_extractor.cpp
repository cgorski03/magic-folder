#include "magic_core/extractors/plaintext_extractor.hpp"


bool PlainTextExtractor::can_handle(const std::filesystem::path& file_path) const {
    return file_path.extension() == ".txt";
}

std::vector<Chunk> PlainTextExtractor::get_chunks(const std::filesystem::path& file_path) const {
    // TODO
    return {};
}
