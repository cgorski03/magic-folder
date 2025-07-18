#pragma once
#include <filesystem>
#include <memory>
#include <vector>

#include "content_extractor.hpp"

/**
 * @class ContentExtractorFactory
 * @brief Manages and provides the correct ContentExtractor for a given file type.
 *
 * This factory holds a collection of all available content extractors.
 * It selects the most appropriate one based on the file's extension or other
 * properties. This class is non-copyable and non-movable.
 */
namespace magic_core {
class ContentExtractorFactory {
 public:
  /**
   * @brief Constructs the factory and initializes all available extractors.
   */
  ContentExtractorFactory();

  /**
   * @brief Finds and returns the most suitable extractor for the given file.
   *
   * Iterates through the registered extractors and returns the first one that
   * reports it can handle the file. A fallback extractor should always be
   * present to handle unknown file types.
   *
   * @param file_path The path to the file that needs to be processed.
   * @return A constant reference to the appropriate ContentExtractor.
   * @throw std::runtime_error if no suitable extractor is found.
   */
  const ContentExtractor& get_extractor_for(const std::filesystem::path& file_path) const;

  // A factory is a unique object that shouldn't be copied or moved.
  // We explicitly delete these operations to prevent misuse.
  // Note: The copy operations would be implicitly deleted anyway because
  // std::unique_ptr is not copyable, but being explicit is clearer.
  ContentExtractorFactory(const ContentExtractorFactory&) = delete;
  ContentExtractorFactory& operator=(const ContentExtractorFactory&) = delete;
  ContentExtractorFactory(ContentExtractorFactory&&) = delete;
  ContentExtractorFactory& operator=(ContentExtractorFactory&&) = delete;

 private:
  // The private members are now declared directly in the class.
  // Any change to this line will trigger recompilation of including files.
  std::vector<std::unique_ptr<ContentExtractor>> extractors;
};
}  // namespace magic_core