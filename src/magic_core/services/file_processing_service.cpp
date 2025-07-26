#include "magic_core/services/file_processing_service.hpp"

#include "magic_core/db/metadata_store.hpp"

namespace magic_core {

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

magic_core::ProcessFileResult FileProcessingService::process_file(
    const std::filesystem::path& file_path) {
  const magic_core::ContentExtractor& extractor =
      content_extractor_factory_->get_extractor_for(file_path);
  auto extraction_result = extractor.extract_with_hash(file_path);

  magic_core::BasicFileMetadata basic_file_metadata;

  basic_file_metadata.path = file_path;
  basic_file_metadata.file_size = std::filesystem::file_size(file_path);
  basic_file_metadata.last_modified = to_sys_time(std::filesystem::last_write_time(file_path));
  basic_file_metadata.created_at = to_sys_time(std::filesystem::last_write_time(file_path));
  basic_file_metadata.original_path = file_path;
  basic_file_metadata.processing_status = "PROCESSING";
  basic_file_metadata.tags = "";
  basic_file_metadata.file_type = extraction_result.file_type;
  // From same read
  basic_file_metadata.file_hash = extraction_result.content_hash;

  int file_id = metadata_store_->create_file_stub(basic_file_metadata);

  // Use chunks from same read (no additional file I/O)
  std::vector<magic_core::ChunkWithEmbedding> chunks_with_embedding = {};

  // init document embedding accumulator
  std::vector<float> document_embedding(MetadataStore::VECTOR_DIMENSION, 0.0f);
  int total_chunks_processed = 0;

  // This will be made more efficient with thread pool in future
  for (const auto& chunk : extraction_result.chunks) {
    std::vector<float> embedding = ollama_client_->get_embedding(chunk.content);
    chunks_with_embedding.push_back({chunk, embedding});

    // Accumulate for document-level embedding (running sum)
    for (size_t i = 0; i < MetadataStore::VECTOR_DIMENSION; ++i) {
      document_embedding[i] += embedding[i];
    }
    total_chunks_processed++;

    // check here if the length of this vector is the batch size
    if (chunks_with_embedding.size() == 64) {
      // do batches of 64 to not use too much memory
      metadata_store_->upsert_chunk_metadata(file_id, chunks_with_embedding);
      chunks_with_embedding.clear();
    }
  }
  // Get whatever was left in the vector
  metadata_store_->upsert_chunk_metadata(file_id, chunks_with_embedding);

  if (total_chunks_processed > 0) {
    // normalize for better similarity matching
    float norm = 0.0f;
    for (float val : document_embedding) {
      norm += val * val;
    }
    norm = std::sqrt(norm);

    if (norm > 0.0f) {
      for (float& val : document_embedding) {
        val /= norm;
      }
    }

    // Store the document-level summary embedding (only if we had chunks)
    metadata_store_->update_file_ai_analysis(file_id, document_embedding, "", "");
    
  } else {
    // For empty files, don't store any embedding
    metadata_store_->update_file_ai_analysis(file_id, {}, "", "");
  }

  return ProcessFileResult::success_response(file_path, basic_file_metadata.file_size,
                                             basic_file_metadata.file_hash,
                                             basic_file_metadata.file_type);
}
}  // namespace magic_core