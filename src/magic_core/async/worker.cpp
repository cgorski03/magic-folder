#include "magic_core/async/worker.hpp"

#include <iostream>

#include "magic_core/async/task_factory.hpp"
#include "magic_core/async/service_provider.hpp"
#include "magic_core/db/task_queue_repo.hpp"
#include "magic_core/async/ITask.hpp"
#include "magic_core/db/models/task_dto.hpp"

namespace magic_core {
namespace async {

Worker::Worker(int worker_id,
               ServiceProvider& services)
    : worker_id_(worker_id),
      services_(services) {
  std::cout << "Worker [" << worker_id_ << "] created." << std::endl;
}

Worker::~Worker() {
  std::cout << "Worker [" << worker_id_ << "] shutting down..." << std::endl;
  // Ensure the stop flag is set before we attempt to join.
  stop();
  // The destructor will block here until the thread has finished its work.
  // This is the RAII pattern for thread management.
  if (thread.joinable()) {
    thread.join();
  }
  std::cout << "Worker [" << worker_id_ << "] joined and shut down." << std::endl;
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
  std::cout << "Worker [" << worker_id_ << "] starting run loop." << std::endl;
  TaskQueueRepo& task_repo = services_.get_task_queue_repo();

  while (!should_stop.load()) {
    std::optional<TaskDTO> task_dto = task_repo.fetch_and_claim_next_task();

    if (task_dto) {
      ITaskPtr task = nullptr;
      try {
        task = TaskFactory::create_task(*task_dto);

        ProgressUpdater on_progress = [&](float p, const std::string& msg) {
          task_repo.upsert_task_progress(task_dto->id, p, msg);
        };

        task->execute(services_, on_progress);
        task_repo.update_task_status(task_dto->id, TaskStatus::COMPLETED);

      } catch (const std::exception& e) {
        std::cerr << "Worker [" << worker_id_ << "] ERROR processing task " << task_dto->id << ": "
                  << e.what() << std::endl;
        task_repo.mark_task_as_failed(task_dto->id, e.what());
      }
    } else {
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }
  std::cout << "Worker [" << worker_id_ << "] run loop terminated." << std::endl;
}

bool Worker::run_one_task() {
  std::cout << "Worker [" << worker_id << "] running a single synchronous cycle..." << std::endl;

  std::optional<TaskDTO> task_opt = task_queue_repo.fetch_and_claim_next_task();

  if (task_opt.has_value()) {
    std::cout << "Worker [" << worker_id << "] found task for file: " << task_opt->file_path
              << std::endl;
    try {
      execute_processing_task(*task_opt);
    } catch (const std::exception& e) {
      task_queue_repo.mark_task_as_failed(task_opt->id, e.what());
      std::cerr << "Worker [" << worker_id << "] CRITICAL ERROR processing file "
                << task_opt->file_path << ": " << e.what() << std::endl;
      return true;
    }
    return true;
  } else {
    std::cout << "Worker [" << worker_id << "] found no pending tasks." << std::endl;
    return false;
  }
}
}
}  // namespace magic_core