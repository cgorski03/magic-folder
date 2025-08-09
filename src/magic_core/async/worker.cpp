#include "magic_core/async/worker.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cmath>

#include "magic_core/db/metadata_store.hpp"
#include "magic_core/extractors/content_extractor_factory.hpp"
#include "magic_core/llm/ollama_client.hpp"
#include "magic_core/services/compression_service.hpp"
#include "magic_core/db/task_queue_repo.hpp"

namespace magic_core {
namespace async {

Worker::Worker(int worker_id,
               MetadataStore& store,
               TaskQueueRepo& task_queue,
               OllamaClient& ollama,
               ContentExtractorFactory& factory)
    : worker_id(worker_id),
      metadata_store(store),
      task_queue_repo(task_queue),
      ollama_client(ollama),
      extractor_factory(factory) {
  std::cout << "Worker [" << worker_id << "] created." << std::endl;
}

Worker::~Worker() {
  std::cout << "Worker [" << worker_id << "] shutting down..." << std::endl;
  // Ensure the stop flag is set before we attempt to join.
  stop();
  // The destructor will block here until the thread has finished its work.
  // This is the RAII pattern for thread management.
  if (thread.joinable()) {
    thread.join();
  }
  std::cout << "Worker [" << worker_id << "] joined and shut down." << std::endl;
}

void Worker::start() {
  if (thread.joinable()) {
    throw std::runtime_error("Worker is already running.");
  }
  should_stop.store(false);
  // Launch the thread, passing a pointer to this object instance.
  // The thread will begin execution in the run_loop() method.
  thread = std::thread(&Worker::run_loop, this);
}

void Worker::stop() {
  // This is a thread-safe way to signal the loop to terminate.
  should_stop.store(true);
}

void Worker::run_loop() {
  std::cout << "Worker [" << worker_id << "] starting run loop." << std::endl;

  // The loop continues as long as the stop flag is false.
  while (!should_stop.load()) {
    // Fetch and claim a task in one atomic operation.
    std::optional<Task> task_opt = task_queue_repo.fetch_and_claim_next_task();

    if (task_opt.has_value()) {
      // If a task was found, execute it.
      std::cout << "Worker [" << worker_id << "] found task for file: " << task_opt->file_path
                << std::endl;
      execute_processing_task(*task_opt);
    } else {
      // If no task was found, sleep to prevent busy-waiting and consuming CPU.
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }

  std::cout << "Worker [" << worker_id << "] run loop terminated." << std::endl;
}

bool Worker::run_one_task() {
  std::cout << "Worker [" << worker_id
            << "] running a single synchronous cycle..." << std::endl;

  std::optional<Task> task_opt = task_queue_repo.fetch_and_claim_next_task();

  if (task_opt.has_value()) {
      std::cout << "Worker [" << worker_id << "] found task for file: "
                << task_opt->file_path << std::endl;
      execute_processing_task(*task_opt);
      return true;
  } else {
      std::cout << "Worker [" << worker_id << "] found no pending tasks."
                << std::endl;
      return false;
  }
}

void Worker::execute_processing_task(const Task& task) {
  long long file_id = -1;
  try {
    std::optional<FileMetadata> file_metadata = metadata_store.get_file_metadata(task.file_path);
    if (!file_metadata.has_value()) {
      throw std::runtime_error("Could not find file stub for path: " + task.file_path);
    }
    
    file_id = file_metadata->id;
    metadata_store.update_file_processing_status(file_metadata->id, ProcessingStatus::PROCESSING);

    const magic_core::ContentExtractor& extractor =
        extractor_factory.get_extractor_for(file_metadata->path);
    auto extraction_result = extractor.extract_with_hash(file_metadata->path);
    // Use chunks from same read (no additional file I/O)
    std::vector<magic_core::ProcessedChunk> processed_chunks = {};

    // init document embedding accumulator
    std::vector<float> document_embedding(MetadataStore::VECTOR_DIMENSION, 0.0f);
    int total_chunks_processed = 0;
    
    // This will be made more efficient with thread pool in future
    for (auto& chunk : extraction_result.chunks) {
      chunk.vector_embedding = ollama_client.get_embedding(chunk.content);

      if (chunk.vector_embedding.empty()) {
        throw std::runtime_error("Received empty embedding vector for chunk: " + chunk.content);
      }

      processed_chunks.push_back({chunk, CompressionService::compress(chunk.content)});

      // Accumulate for document-level embedding (running sum)
      for (size_t i = 0; i < MetadataStore::VECTOR_DIMENSION; ++i) {
        document_embedding[i] += chunk.vector_embedding[i];
      }
      total_chunks_processed++;

      // check here if the length of this vector is the batch size
      if (processed_chunks.size() == 64) {
        // do batches of 64 to not use too much memory
        metadata_store.upsert_chunk_metadata(file_id, processed_chunks);
        processed_chunks.clear();
      }
    }
    // Get whatever was left in the vector
    if (!processed_chunks.empty()) {
      metadata_store.upsert_chunk_metadata(file_id, processed_chunks);
    }

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
      metadata_store.update_file_ai_analysis(file_id, document_embedding, "", "",
                                             ProcessingStatus::PROCESSED);
      metadata_store.rebuild_faiss_index();
    }
    
    // Mark task as completed
    task_queue_repo.update_task_status(task.id, TaskStatus::COMPLETED);
    
  } catch (const std::exception& e) {
    std::cerr << "Worker [" << worker_id << "] CRITICAL ERROR processing file "
              << task.file_path << ": " << e.what() << std::endl;

    // If an error occurs, update the status in both tables for diagnosis.
    if (file_id != -1) {
      metadata_store.update_file_ai_analysis(file_id, {}, "", "", ProcessingStatus::FAILED);
    }
    task_queue_repo.mark_task_as_failed(task.id, e.what());
  }
}

}  // namespace async
}  // namespace magic_core