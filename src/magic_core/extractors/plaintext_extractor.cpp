#include "magic_core/extractors/plaintext_extractor.hpp"
#include <sstream>
#include <regex>

namespace magic_core {

bool PlainTextExtractor::can_handle(const std::filesystem::path& file_path) const {
    const std::string extension = file_path.extension().string();
    return extension == ".txt";
}

// New combined method - single file read for both hash and chunks
ExtractionResult PlainTextExtractor::extract_with_hash(const std::filesystem::path& file_path) const {
    std::string content = get_string_content(file_path);

    if (content.empty()) {
        return {"", {}, FileType::Text};
    }

    // Compute hash from loaded content (no additional file read!)
    std::string content_hash = compute_hash_from_content(content);
    
    // Extract chunks from the same loaded content
    std::vector<Chunk> chunks = extract_chunks_from_content(content);
    
    return {content_hash, chunks, FileType::Text};
}

// Legacy method - now uses the new architecture
std::vector<Chunk> PlainTextExtractor::get_chunks(const std::filesystem::path& file_path) const {
    auto result = extract_with_hash(file_path);
    return result.chunks;
}

/**
 * @brief Helper method that processes already-loaded content.
 *
 * This method splits the document by paragraphs (defined as one or more
 * blank lines). It then merges paragraphs that are too short and breaks up
 * paragraphs that are too long, ensuring well-sized, meaningful chunks.
 */
std::vector<Chunk> PlainTextExtractor::extract_chunks_from_content(const std::string& content) const {
    if (content.empty()) {
        return {};
    }

    // This regex finds one or more blank lines, which act as paragraph separators.
    // \n\s*\n matches a newline, followed by any whitespace, followed by another newline.
    const std::regex paragraph_regex(R"(\n\s*\n)", std::regex_constants::ECMAScript | std::regex_constants::multiline);

    std::vector<long> split_points;
    split_points.push_back(0);

    auto sections_begin = std::sregex_iterator(content.begin(), content.end(), paragraph_regex);
    auto sections_end = std::sregex_iterator();

    for (std::sregex_iterator i = sections_begin; i != sections_end; ++i) {
        // We want to split *after* the blank lines, so we add the match length.
        long split_pos = i->position() + i->length();
        split_points.push_back(split_pos);
    }
    split_points.push_back(content.length());
    
    // The merging and fallback logic is identical to the robust MarkdownExtractor
    std::vector<Chunk> final_chunks;
    int current_chunk_index = 0;
    std::stringstream merged_chunk_buffer;

    for (size_t i = 0; i < split_points.size() - 1; ++i) {
        long start = split_points[i];
        long end = split_points[i + 1];
        long length = end - start;

        if (length == 0) continue;

        std::string semantic_section = content.substr(start, length);

        merged_chunk_buffer << semantic_section;

        if (merged_chunk_buffer.str().length() >= MIN_CHUNK_SIZE || i == split_points.size() - 2) {
            std::string current_chunk_content = merged_chunk_buffer.str();
            // If the merged chunk is too large, apply the fixed-size fallback
            if (current_chunk_content.length() > MAX_CHUNK_SIZE) {
                std::vector<std::string> smaller_chunks = split_into_fixed_chunks(current_chunk_content);
                for (const auto& small_chunk : smaller_chunks) {
                    final_chunks.push_back({.content = small_chunk, .chunk_index = current_chunk_index++});
                }
            } else {
                // The chunk is a good size, add it directly.
                final_chunks.push_back({.content = current_chunk_content, .chunk_index = current_chunk_index++});
            }
            
            merged_chunk_buffer.str("");
            merged_chunk_buffer.clear();
        }
    }

    return final_chunks;
}

} // namespace magic_core