#include "magic_core/async/worker_pool.hpp"
#include <iostream>

namespace magic_core::async {

WorkerPool::WorkerPool(size_t num_threads, MetadataStore& store, OllamaClient& ollama,
                       ContentExtractorFactory& factory) {
    if (num_threads == 0) {
        throw std::invalid_argument("WorkerPool must have at least one thread.");
    }

    // Reserve space in the vector for efficiency
    m_workers.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        m_workers.emplace_back(std::make_unique<Worker>(
            static_cast<int>(i), store, ollama, factory));
    }
    std::cout << "WorkerPool created with " << num_threads << " workers."
              << std::endl;
}

WorkerPool::~WorkerPool() {
    std::cout << "WorkerPool destructor called. Shutting down all workers..."
              << std::endl;
    if (m_is_running) {
        stop();
    }
}

void WorkerPool::start() {
    if (m_is_running) {
        std::cerr << "Warning: WorkerPool is already running." << std::endl;
        return;
    }
    std::cout << "Starting all workers in the pool..." << std::endl;
    for (const auto& worker : m_workers) {
        worker->start();
    }
    m_is_running = true;
}

void WorkerPool::stop() {
    if (!m_is_running) {
        return;
    }
    std::cout << "Stopping all workers in the pool..." << std::endl;
    for (const auto& worker : m_workers) {
        worker->stop();
    }
    m_is_running = false;
}

} // namespace magic_core::async