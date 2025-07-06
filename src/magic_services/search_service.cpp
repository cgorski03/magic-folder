#include "magic_services/search_service.hpp"

namespace magic_services {

SearchService::SearchService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                             std::shared_ptr<magic_core::OllamaClient> ollama_client)
    : metadata_store_(metadata_store), ollama_client_(ollama_client) {}

std::vector<magic_core::SearchResult> SearchService::search(const std::string &query, int k) {
  // TODO: Implement actual search
  throw new SearchServiceException("Not implemented yet!!");
}

std::vector<float> SearchService::embed_query(const std::string &query) {
  return ollama_client_->get_embedding(query);
}
}  // namespace magic_services