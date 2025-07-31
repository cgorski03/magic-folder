#include "magic_core/extractors/content_extractor.hpp"
#include <utf8.h>
#include <openssl/evp.h>

#include <fstream>
#include <iomanip>
#include <sstream>

namespace magic_core {

std::string ContentExtractor::get_string_content(const fs::path& file_path) const {
  std::ifstream file_stream(file_path);
  if (!file_stream.is_open()) {
    throw ContentExtractorError("Could not open file: " + file_path.string());
  }

  std::stringstream buffer;
  buffer << file_stream.rdbuf();
  const std::string content = buffer.str();
  return content;
}

// New helper method for computing hash from already-loaded content
std::string ContentExtractor::compute_hash_from_content(const std::string& content) const {
  EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    throw ContentExtractorError("Failed to create EVP context for hashing");
  }

  if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw ContentExtractorError("Failed to initialize SHA256 digest");
  }

  // Hash the entire content at once (no file I/O needed)
  if (EVP_DigestUpdate(mdctx, content.data(), content.length()) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw ContentExtractorError("Failed to update SHA256 digest");
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw ContentExtractorError("Failed to finalize SHA256 digest");
  }

  EVP_MD_CTX_free(mdctx);

  std::stringstream ss;
  for (unsigned int i = 0; i < hash_len; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }

  return ss.str();
}

// Legacy method - now uses the new architecture
std::string ContentExtractor::get_content_hash(const fs::path& file_path) const {
  std::string content = get_string_content(file_path);
  return compute_hash_from_content(content);
}

/**
 * @brief Implements the fixed-size chunking strategy as a fallback.
 *
 */
std::vector<std::string> ContentExtractor::split_into_fixed_chunks(const std::string& text) const {
  std::vector<std::string> out;
  if (text.empty())
    return out;

  size_t bytes_in_chunk = 0;
  auto chunk_start = text.begin();

  for (auto it = text.begin(); it != text.end(); utf8::next(it, text.end())) {
    bytes_in_chunk = it - chunk_start;
    if (bytes_in_chunk >= FIXED_CHUNK_SIZE - OVERLAP_SIZE) {
      out.emplace_back(chunk_start, it);  // up to, but *not* including it
      chunk_start = it;
    }
  }
  // last chunk
  if (chunk_start != text.end())
    out.emplace_back(chunk_start, text.end());

  return out;
}
}  // namespace magic_core