#pragma once
#include <filesystem>
#include <vector>

#include "magic_core/extractors/content_extractor.hpp"
#include "magic_core/types/chunk.hpp"

namespace magic_core {

class MacPdfExtractor : public ContentExtractor {
 public:
  explicit MacPdfExtractor() {}

  bool can_handle(const std::filesystem::path& file_path) const override {
#if defined(__APPLE__)
    auto ext = file_path.extension().string();
    for (auto& c : ext)
      c = (char)tolower(c);
    return ext == ".pdf";
#else
    return false;
#endif
  }

  std::vector<Chunk> get_chunks(const std::filesystem::path& file_path) const override;
  
  ExtractionResult extract_with_hash(const std::filesystem::path& file_path) const override;

 private:
};

}  // namespace magic_core