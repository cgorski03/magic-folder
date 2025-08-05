#include "magic_core/services/search_service.hpp"
#include "magic_core/services/compression_service.hpp"
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
  std::vector<float> qvec = embed_query(query);
  auto file_hits = metadata_store_->search_similar_files(qvec, k);
  auto chunk_hits = metadata_store_->search_similar_chunks(get_file_ids(file_hits), qvec, k);

  std::vector<ChunkResultDTO> chunk_dtos;
  chunk_dtos.reserve(chunk_hits.size());

  for (const auto &hit : chunk_hits) {
    ChunkResultDTO dto;
    dto.id = hit.id;
    dto.distance = hit.distance;
    dto.file_id = hit.file_id;
    dto.chunk_index = hit.chunk_index;
    dto.content = CompressionService::decompress(hit.compressed_content);
    chunk_dtos.push_back(std::move(dto));
  }

  return {std::move(file_hits), std::move(chunk_dtos)};
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