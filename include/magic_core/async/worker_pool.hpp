#pragma once

#include "magic_core/async/worker.hpp"
#include <memory>
#include <vector>

namespace magic_core {
class MetadataStore;
class OllamaClient;
class ContentExtractorFactory;
}

namespace magic_core::async {

/**
 * @class WorkerPool
 * @brief Manages a collection of Worker threads for concurrent task processing.
 *
 * This class is responsible for the entire lifecycle of the worker threads:
 * creating them, starting them, and ensuring they are safely shut down
 * when the pool is destroyed. It follows the RAII principle.
 */
class WorkerPool {
public:
    /**
     * @brief Constructs the WorkerPool and creates the worker instances.
     *
     * @param num_threads The number of worker threads to create in the pool.
     * @param queue A reference to the shared task queue repository.
     * @param store A reference to the shared metadata store.
     * @param ollama A reference to the shared Ollama client.
     * @param factory A reference to the shared content extractor factory.
     */
    WorkerPool(size_t num_threads, MetadataStore& store, OllamaClient& ollama,
               ContentExtractorFactory& factory);

    /**
     * @brief Destructor. Automatically stops and joins all worker threads.
     */
    ~WorkerPool();

    /**
     * @brief Starts all worker threads in the pool.
     *
     * Each worker will begin polling the task queue in its own thread.
     */
    void start();

    /**
     * @brief Signals all worker threads in the pool to stop.
     *
     * The workers will finish their current tasks and then exit their loops.
     * This method does not block. The destructor ensures waiting is handled.
     */
    void stop();

    // --- Rule of Five: Make the class non-copyable and non-movable ---
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&) = delete;
    WorkerPool& operator=(WorkerPool&&) = delete;

private:
    std::vector<std::unique_ptr<Worker>> m_workers;
    bool m_is_running = false;
};

} // namespace magic_core::async