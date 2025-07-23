#pragma once

#include <memory>
#include <string>
#include <vector>

#include "magic_core/db/metadata_store.hpp"
#include "magic_core/llm/ollama_client.hpp"

namespace magic_core {

class SearchServiceException : public std::exception {
 public:
  SearchServiceException(const std::string &message) : message_(message) {}
  const char *what() const noexcept override {
    return message_.c_str();
  }

 private:
  std::string message_;
};

class SearchService {
 public:
  SearchService(std::shared_ptr<MetadataStore> metadata_store,
                std::shared_ptr<OllamaClient> ollama_client);

  // Natural-language semantic search. Returns top-k nearest neighbours.
  std::vector<SearchResult> search(const std::string &query, int k = 10);

 private:
  std::vector<float> embed_query(const std::string &query);

  std::shared_ptr<MetadataStore> metadata_store_;
  std::shared_ptr<OllamaClient> ollama_client_;
};

}  // namespace magic_core