#include "magic_core/extractors/pdf_extractor_mac.hpp"

#include <algorithm>
#include <string>
#include <vector>

#import <Foundation/Foundation.h>
#import <PDFKit/PDFKit.h>
#include "magic_core/types/chunk.hpp"


namespace {
std::string ns_to_utf8(NSString* s) {
  if (!s) return {};
  const char* c = [s UTF8String];
  return c ? std::string(c) : std::string();
}

std::vector<magic_core::Chunk> chunk_text(const std::string& text,
                                          size_t max_chars,
                                          size_t overlap,
                                          const std::string& source_path) {
  std::vector<magic_core::Chunk> out;
  if (text.empty() || max_chars == 0) return out;
  size_t start = 0;
  int idx = 0;
  while (start < text.size()) {
    size_t end = std::min(text.size(), start + max_chars);
    magic_core::Chunk c;
    c.content.assign(text.data() + start, end - start);
    c.chunk_index = idx++;
    out.emplace_back(std::move(c));
    if (end == text.size()) break;
    size_t step = max_chars - std::min(overlap, max_chars);
    start += step;
  }
  return out;
}
}  // namespace

namespace magic_core {

std::vector<Chunk> MacPdfExtractor::get_chunks(
    const std::filesystem::path& file_path) const {
#if !defined(__APPLE__)
  return {};
#else
  @autoreleasepool {
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:file_path.string().c_str()]];
    if (!url) throw std::runtime_error("Invalid PDF path: " + file_path.string());

    PDFDocument* doc = [[PDFDocument alloc] initWithURL:url];
    if (!doc) throw std::runtime_error("Failed to open PDF (PDFKit): " + file_path.string());

    const NSInteger pages = [doc pageCount];
    std::string all;
    all.reserve(4096);

    for (NSInteger i = 0; i < pages; ++i) {
      PDFPage* page = [doc pageAtIndex:i];
      if (!page) continue;
      NSString* s = [page string]; // Extracts text layer; nil if none
      std::string page_text = ns_to_utf8(s);
      if (!page_text.empty()) {
        all.append(page_text);
        all.append("\n\n--- Page ");
        all.append(std::to_string((int)i + 1));
        all.append(" ---\n\n");
      }
    }

    // No OCR here; scanned PDFs will yield empty text
    if (all.empty()) return {};

    return chunk_text(all, ContentExtractor::MAX_CHUNK_SIZE, ContentExtractor::OVERLAP_SIZE,
                      file_path.string());
  }
#endif
}

ExtractionResult MacPdfExtractor::extract_with_hash(const std::filesystem::path& file_path) const {
  ExtractionResult result;
  result.chunks = get_chunks(file_path);
  result.content_hash = get_content_hash(file_path);
  return result;
}

}  // namespace magic_core