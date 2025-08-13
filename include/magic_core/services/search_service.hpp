#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "magic_core/db/metadata_store.hpp"
#include "magic_core/llm/ollama_client.hpp"

namespace magic_core {

class SearchService {
 public:
  struct ChunkResultDTO {
    int id;
    float distance;
    int file_id;
    int chunk_index;
    std::string content;
  };
  struct MagicSearchResult {
    std::vector<FileSearchResult> file_results;
    std::vector<ChunkResultDTO> chunk_results;
  };
  SearchService(std::shared_ptr<MetadataStore> metadata_store,
                std::shared_ptr<OllamaClient> ollama_client,
                std::function<std::string(const std::vector<char>&)> decompress_fn = {});

  // Natural-language semantic search. Returns top-k nearest neighbours.
  std::vector<FileSearchResult> search_files(const std::string &query, int k = 10);
  MagicSearchResult search(const std::string &query, int k = 10);

 private:
  std::vector<float> embed_query(const std::string &query);
  std::vector<int> get_file_ids(const std::vector<FileSearchResult> &file_results);
  std::shared_ptr<MetadataStore> metadata_store_;
  std::shared_ptr<OllamaClient> ollama_client_;
  std::function<std::string(const std::vector<char>&)> decompress_fn_;
};

}  // namespace magic_core