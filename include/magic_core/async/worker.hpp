#pragma once

#include <atomic>
#include <thread>
#include "magic_core/db/task.hpp"

namespace magic_core {
class MetadataStore;
class OllamaClient;
struct Task;
class ContentExtractorFactory;
class TaskQueueRepo;
}

namespace magic_core {
namespace async {

/**
 * @class Worker
 * @brief Represents a single background thread that processes tasks from the queue.
 *
 * A Worker is a long-lived object that continuously polls the TaskQueue for
 * pending jobs. When a job is found, the Worker executes the necessary
 * file processing logic (chunking, embedding, etc.).
 *
 * This class is designed to be managed by a WorkerPool. It is non-copyable
 * and non-movable to ensure clear ownership of the underlying thread.
 */
 class Worker {
  public:
      /**
       * @brief Constructs a Worker instance.
       * @param worker_id A unique identifier for this worker, used for logging.
       * @param queue A reference to the task queue repository for fetching jobs.
       * @param store A reference to the metadata store for DB writes.
       * @param ollama A reference to the Ollama client for AI operations.
       * @param factory A reference to the content extractor factory.
       */
      Worker(int worker_id, MetadataStore& store, TaskQueueRepo& task_queue, OllamaClient& ollama, ContentExtractorFactory& factory);
  
      /**
       * @brief Destructor. Ensures the worker thread is stopped and joined cleanly.
       *
       * This is a critical part of the design. It calls stop() and then join()
       * on the underlying thread, guaranteeing that the application does not
       * terminate while the worker is still running.
       */
      ~Worker();
  
      /**
       * @brief Starts the worker's processing loop in a new background thread.
       *
       * This method will throw an exception if the worker is already running.
       */
      void start();
  
      /**
       * @brief Signals the worker to stop processing after its current task.
       *
       * This method sets a flag that the worker's loop checks. The thread will
       * finish its current task (if any) and then exit gracefully. This method
       * does NOT block; use the destructor or a separate join() method to wait.
       */
      void stop();
  
      Worker(const Worker&) = delete;
      Worker& operator=(const Worker&) = delete;
      Worker(Worker&&) = delete;
      Worker& operator=(Worker&&) = delete;
      bool run_one_task();
  
  private:
      /**
       * @brief The main loop for the worker thread.
       *
       * This function continuously polls the task queue, executes tasks, and
       * sleeps when no work is available. It runs until stop() is called.
       */
      void run_loop();

      /**
       * @brief Executes the full processing pipeline for a single task.
       * @param task The task object fetched from the queue.
       */
       void execute_processing_task(const Task& task);
  
      int worker_id;
  
      MetadataStore& metadata_store;
      TaskQueueRepo& task_queue_repo;
      OllamaClient& ollama_client;
      ContentExtractorFactory& extractor_factory;
  
      std::atomic<bool> should_stop{false};
      std::thread thread;
  };
}
}