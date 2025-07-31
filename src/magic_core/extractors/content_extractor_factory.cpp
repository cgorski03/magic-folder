#include "magic_core/extractors/content_extractor_factory.hpp"
#include "magic_core/extractors/markdown_extractor.hpp"
#include "magic_core/extractors/plaintext_extractor.hpp"
#include "magic_core/extractors/content_extractor.hpp"

#include <stdexcept>

namespace magic_core {
ContentExtractorFactory::ContentExtractorFactory() {
    extractors.push_back(std::make_unique<MarkdownExtractor>());
    extractors.push_back(std::make_unique<PlainTextExtractor>());
}

const ContentExtractor& ContentExtractorFactory::get_extractor_for(
    const std::filesystem::path& file_path) const {
    for (const auto& extractor : extractors) {
        if (extractor->can_handle(file_path)) {
            return *extractor;
        }
    }
    throw std::runtime_error("No suitable content extractor found for " +
                             file_path.string());
}
}