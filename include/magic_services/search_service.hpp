#pragma once

#include <magic_core/metadata_store.hpp>
#include <magic_core/ollama_client.hpp>
#include <memory>
#include <string>
#include <vector>

namespace magic_services {

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
  SearchService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                std::shared_ptr<magic_core::OllamaClient> ollama_client);

  // Natural-language semantic search. Returns top-k nearest neighbours.
  std::vector<magic_core::SearchResult> search(const std::string &query, int k = 10);

 private:
  std::vector<float> embed_query(const std::string &query);

  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::shared_ptr<magic_core::OllamaClient> ollama_client_;
};

}  // namespace magic_services