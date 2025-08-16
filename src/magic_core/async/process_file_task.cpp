#include "magic_core/async/process_file_task.hpp"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "magic_core/async/service_provider.hpp"
#include "magic_core/db/metadata_store.hpp"
#include "magic_core/extractors/content_extractor.hpp"
#include "magic_core/extractors/content_extractor_factory.hpp"
#include "magic_core/llm/ollama_client.hpp"
#include "magic_core/services/compression_service.hpp"
#include "magic_core/types/chunk.hpp"

namespace magic_core {

ProcessFileTask::ProcessFileTask(long long id,
                                 TaskStatus status,
                                 std::chrono::system_clock::time_point created_at,
                                 std::chrono::system_clock::time_point updated_at,
                                 std::optional<std::string> error_message,
                                 std::string file_path)
    : ITask(id, status, created_at, updated_at, error_message), file_path_(std::move(file_path)) {}

void ProcessFileTask::execute(ServiceProvider& services, const ProgressUpdater& on_progress) {
  on_progress(0.0f, "Starting processing...");

  // 1. Get file metadata:
  MetadataStore& store = services.get_metadata_store();
  std::optional<FileMetadata> metadata = store.get_file_metadata(file_path_);
  if (!metadata) {
    throw std::runtime_error("Could not find file metadata for path: " + file_path_);
  }
  store.update_file_processing_status(metadata->id, ProcessingStatus::PROCESSING);
  on_progress(0.05f, "File metadata loaded.");

  // 2. Extract content and chunks
  ContentExtractorFactory& factory = services.get_extractor_factory();
  const ContentExtractor& extractor = factory.get_extractor_for(metadata->path);
  ExtractionResult extraction_result = extractor.extract_with_hash(metadata->path);
  on_progress(0.1f, "Content extracted.");

  // 3. Process chunks, get embeddings, and save in batches
  process_chunks_in_batches(metadata->id, extraction_result.chunks, services, on_progress);

  // 4. Calculate and store the final document-level embedding
  finalize_document_embedding(metadata->id, extraction_result.chunks, store);
  on_progress(0.95f, "Document summary embedding stored.");

  // 5. Finalize state
  store.rebuild_faiss_index();  // Consider if this can be done less frequently
  on_progress(1.0f, "Processing complete.");
}

// --- Private Helper Methods for Clarity ---

void ProcessFileTask::process_chunks_in_batches(long long file_id,
                                                std::vector<Chunk>& chunks,
                                                ServiceProvider& services,
                                                const ProgressUpdater& on_progress) {
  auto& ollama = services.get_ollama_client();
  auto& store = services.get_metadata_store();
  std::vector<ProcessedChunk> batch;
  const size_t BATCH_SIZE = 64;
  batch.reserve(BATCH_SIZE);

  for (size_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    chunk.vector_embedding = ollama.get_embedding(chunk.content);

    if (chunk.vector_embedding.empty()) {
      throw std::runtime_error("Received empty embedding for a chunk.");
    }

    batch.push_back({chunk, CompressionService::compress(chunk.content)});

    if (batch.size() >= BATCH_SIZE) {
      store.upsert_chunk_metadata(file_id, batch);
      batch.clear();
    }

    if (i % 10 == 0) {
      float progress = 0.1f + (0.8f * (static_cast<float>(i + 1) / chunks.size()));
      std::string message =
          "Embedding chunk " + std::to_string(i + 1) + " of " + std::to_string(chunks.size());
      on_progress(progress, message);
    }
  }

  if (!batch.empty()) {
    store.upsert_chunk_metadata(file_id, batch);
  }
}

void ProcessFileTask::finalize_document_embedding(long long file_id,
                                                  const std::vector<Chunk>& chunks,
                                                  MetadataStore& store) {
  if (chunks.empty()) {
    // If there's no content, just mark as processed.
    store.update_file_processing_status(file_id, ProcessingStatus::PROCESSED);
    return;
  }

  std::vector<float> doc_embedding(MetadataStore::VECTOR_DIMENSION, 0.0f);
  for (const auto& chunk : chunks) {
    for (size_t i = 0; i < doc_embedding.size(); ++i) {
      doc_embedding[i] += chunk.vector_embedding[i];
    }
  }

  // Normalize the final vector
  float norm = 0.0f;
  for (float val : doc_embedding) {
    norm += val * val;
  }
  norm = std::sqrt(norm);
  if (norm > 0.0f) {
    for (float& val : doc_embedding) {
      val /= norm;
    }
  }

  store.update_file_ai_analysis(file_id, doc_embedding, "", "", ProcessingStatus::PROCESSED);
}

}  // namespace magic_core