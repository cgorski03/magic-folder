#include "magic_services/file_processing_service.hpp"
#include "magic_core/metadata_store.hpp"

namespace magic_services {

  auto to_sys_time = [](std::filesystem::file_time_type ftime) {
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
};

FileProcessingService::FileProcessingService(
      std::shared_ptr<magic_core::MetadataStore> metadata_store,
      std::shared_ptr<magic_core::ContentExtractorFactory> content_extractor_factory,
    std::shared_ptr<magic_core::OllamaClient> ollama_client)
    : metadata_store_(metadata_store),
      content_extractor_factory_(content_extractor_factory),
      ollama_client_(ollama_client) {}

magic_services::ProcessFileResult FileProcessingService::process_file(
    const std::filesystem::path &file_path) {
      
      const magic_core::ContentExtractor& extractor = content_extractor_factory_->get_extractor_for(file_path);
      // First, we will insert the basic file metadata
      // compute the content hash
      std::string content_hash = extractor.get_content_hash(file_path);
      magic_core::BasicFileMetadata basic_file_metadata;
      
      basic_file_metadata.path = file_path;
      basic_file_metadata.file_size = std::filesystem::file_size(file_path);
      basic_file_metadata.last_modified = to_sys_time(std::filesystem::last_write_time(file_path));
      basic_file_metadata.created_at = to_sys_time(std::filesystem::last_write_time(file_path));
      basic_file_metadata.original_path = file_path;
      basic_file_metadata.processing_status = "PROCESSING";
      basic_file_metadata.tags = "";
      basic_file_metadata.file_type = magic_core::file_type_from_string(file_path.extension().string());
      basic_file_metadata.content_hash = content_hash;
      
      int file_id = metadata_store_->create_file_stub(basic_file_metadata);
      
      // Then we will get the chunks from the extractor
      std::vector<Chunk> chunks = extractor.get_chunks(file_path);
      
      std::vector<magic_core::ChunkWithEmbedding> chunks_with_embedding = {};

      // Then we will get the embeddings for the chunks
      // This will be made more efficient with thread pool in future
      for (const auto& chunk : chunks) {
        std::vector<float> embedding = ollama_client_->get_embedding(chunk.content);
        chunks_with_embedding.push_back({chunk, embedding});
        // check here if the length of this vector is the batch size
        if (chunks_with_embedding.size() == 64) {
          // do batches of 64 to not use too much memory
          metadata_store_->upsert_chunk_metadata(file_id, chunks_with_embedding);
          chunks_with_embedding.clear();
        }
      }
      // Get whatever was left in the vector
      metadata_store_->upsert_chunk_metadata(file_id, chunks_with_embedding);
      
      // Then we will update the file metadata with the embedding
      // TODO implement this
      metadata_store_->update_file_ai_analysis(file_id, {}, "", "");
      
    return ProcessFileResult::success_response(file_path, basic_file_metadata.file_size,
                                               basic_file_metadata.content_hash,
                                               to_string(basic_file_metadata.file_type));

}
}