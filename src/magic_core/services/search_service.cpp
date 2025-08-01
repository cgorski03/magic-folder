#include "magic_core/services/search_service.hpp"

namespace magic_core {

SearchService::SearchService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                             std::shared_ptr<magic_core::OllamaClient> ollama_client)
    : metadata_store_(metadata_store), ollama_client_(ollama_client) {}

std::vector<magic_core::FileSearchResult> SearchService::search_files(const std::string &query,
                                                                      int k) {
  try {
    // Convert the query to a vector embedding
    std::vector<float> query_embedding = embed_query(query);

    // Step 2: Use the metadata store to search for similar files
    // The metadata store will use the Faiss index to find the most similar vectors
    std::vector<magic_core::FileSearchResult> results =
        metadata_store_->search_similar_files(query_embedding, k);

    return results;
  } catch (const std::exception &e) {
    // Re-throw as SearchServiceException to maintain the expected interface
    throw SearchServiceException("Search failed: " + std::string(e.what()));
  }
}

SearchService::MagicSearchResult SearchService::search(const std::string &query, int k) {
  try {
    std::vector<float> query_embedding = embed_query(query);
    std::vector<magic_core::FileSearchResult> file_results =
        metadata_store_->search_similar_files(query_embedding, k);

    // Step 2: Use the metadata store to search for similar chunks
    std::vector<magic_core::ChunkSearchResult> chunk_results =
        metadata_store_->search_similar_chunks(get_file_ids(file_results), query_embedding, k);
    return {file_results, chunk_results};
  } catch (const std::exception &e) {
    // Re-throw as SearchServiceException to maintain the expected interface
    throw SearchServiceException("Search failed: " + std::string(e.what()));
  }
}
std::vector<float> SearchService::embed_query(const std::string &query) {
  return ollama_client_->get_embedding(query);
}

std::vector<int> SearchService::get_file_ids(const std::vector<FileSearchResult> &file_results) {
  std::vector<int> file_ids;
  for (const auto &file_result : file_results) {
    file_ids.push_back(file_result.id);
  }
  return file_ids;
}
}  // namespace magic_core