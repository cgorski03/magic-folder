#include "magic_core/extractors/markdown_extractor.hpp"

#include <regex>
#include <sstream>
#include "magic_core/types/file.hpp"

namespace magic_core {
bool MarkdownExtractor::can_handle(const std::filesystem::path& file_path) const {
  return file_path.extension() == ".md";
}

// New combined method - single file read for both hash and chunks
ExtractionResult MarkdownExtractor::extract_with_hash(const std::filesystem::path& file_path) const {
  std::string content = get_string_content(file_path);

  if (content.empty()) {
    return {"", {}, FileType::Markdown};
  }

  // Compute hash from loaded content (no additional file read!)
  std::string content_hash = compute_hash_from_content(content);
  
  // Extract chunks from the same loaded content
  std::vector<Chunk> chunks = extract_chunks_from_content(content);
  
  return {content_hash, chunks, FileType::Markdown};
}

// Legacy method - now uses the new architecture
std::vector<Chunk> MarkdownExtractor::get_chunks(const std::filesystem::path& file_path) const {
  auto result = extract_with_hash(file_path);
  return result.chunks;
}

// Helper method that processes already-loaded content
std::vector<Chunk> MarkdownExtractor::extract_chunks_from_content(const std::string& content) const {
  if (content.empty()) {
    return {};
  }

  const std::regex heading_regex(
      R"(^#+\s.*)", std::regex_constants::ECMAScript | std::regex_constants::multiline);

  std::vector<long> split_points;
  split_points.push_back(0);

  auto headings_begin = std::sregex_iterator(content.begin(), content.end(), heading_regex);
  auto headings_end = std::sregex_iterator();

  for (std::sregex_iterator i = headings_begin; i != headings_end; ++i) {
    split_points.push_back(i->position());
  }
  split_points.push_back(content.length());

  std::vector<Chunk> final_chunks;
  int current_chunk_index = 0;
  std::stringstream merged_chunk_buffer;

  for (size_t i = 0; i < split_points.size() - 1; ++i) {
    long start = split_points[i];
    long end = split_points[i + 1];
    long length = end - start;

    if (length == 0)
      continue;

    std::string semantic_section = content.substr(start, length);

    // Add the new section to our temporary buffer
    merged_chunk_buffer << semantic_section;
    

    // Check if the merged buffer is now large enough to be a chunk
    if (merged_chunk_buffer.str().length() >= MIN_CHUNK_SIZE) {
      std::string current_chunk_content = merged_chunk_buffer.str();

      if (current_chunk_content.length() <= MAX_CHUNK_SIZE) {
        final_chunks.push_back(
            {.content = current_chunk_content, .chunk_index = current_chunk_index++});
      } else {
        // The merged chunk is too large, apply the fixed-size fallback
        std::vector<std::string> smaller_chunks = split_into_fixed_chunks(current_chunk_content);
        for (const auto& small_chunk : smaller_chunks) {
          final_chunks.push_back({.content = small_chunk, .chunk_index = current_chunk_index++});
        }
      }

      // Reset the buffer for the next merged chunk
      merged_chunk_buffer.str("");
      merged_chunk_buffer.clear();
    }
  }

  // Handle any remaining content in the buffer
  if (!merged_chunk_buffer.str().empty()) {
    std::string remaining_content = merged_chunk_buffer.str();
    
    if (remaining_content.length() <= MAX_CHUNK_SIZE) {
      final_chunks.push_back({.content = remaining_content, .chunk_index = current_chunk_index++});
    } else {
      // Apply fixed-size fallback for large remaining content
      std::vector<std::string> smaller_chunks = split_into_fixed_chunks(remaining_content);
      for (const auto& small_chunk : smaller_chunks) {
        final_chunks.push_back({.content = small_chunk, .chunk_index = current_chunk_index++});
      }
    }
  } else {
  }

  return final_chunks;
}
}  // namespace magic_core