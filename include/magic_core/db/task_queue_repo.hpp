#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "magic_core/db/database_manager.hpp"
#include "magic_core/db/task.hpp"

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

  long long create_task(const std::string& task_type,
                        const std::string& file_path,
                        int priority = 10);

  std::optional<Task> fetch_and_claim_next_task();

  void update_task_status(long long task_id, TaskStatus new_status);
  void mark_task_as_failed(long long task_id, const std::string& error_message);
  std::vector<Task> get_tasks_by_status(TaskStatus status);
  void clear_completed_tasks(int older_than_days = 7);

 private:
  DatabaseManager& db_manager_;

  static std::string time_point_to_string(const std::chrono::system_clock::time_point& tp);
  static std::chrono::system_clock::time_point string_to_time_point(const std::string& time_str);
};

}  // namespace magic_core
