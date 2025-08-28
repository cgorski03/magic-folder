#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "magic_core/db/database_manager.hpp"
#include "magic_core/db/models/task_dto.hpp"
#include "magic_core/db/models/task_progress_dto.hpp"

namespace magic_core {

class TaskQueueRepoError : public std::exception {
 public:
  explicit TaskQueueRepoError(const std::string& message) : message_(message) {}
  const char* what() const noexcept override {
    return message_.c_str();
  }

 private:
  std::string message_;
};

class TaskQueueRepo {
 public:
  explicit TaskQueueRepo(DatabaseManager& db_manager);

  long long create_file_process_task(const std::string& task_type,
                        const std::string& file_path,
                        int priority = 10);

  std::optional<TaskDTO> fetch_and_claim_next_task();

  void update_task_status(long long task_id, TaskStatus new_status);
  void mark_task_as_failed(long long task_id, const std::string& error_message);
  std::vector<TaskDTO> get_tasks_by_status(TaskStatus status);
  void clear_completed_tasks(int older_than_days = 7);
  void upsert_task_progress(long long task_id, float percent, const std::string& message);
  std::optional<TaskProgressDTO> get_task_progress(long long task_id);

  // File watcher convenience methods
  void enqueue_process_file(const std::string& file_path, int priority = 10);
  void enqueue_reindex_file(const std::string& file_path, int priority = 8);

  // Utility functions for time conversion
  static std::string time_point_to_string(const std::chrono::system_clock::time_point& tp);
  static std::chrono::system_clock::time_point string_to_time_point(const std::string& time_str);

 private:
  DatabaseManager& db_manager_;
};

}  // namespace magic_core
