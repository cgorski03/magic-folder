#pragma once

#include <chrono>
#include <stdexcept>
#include <string>

namespace magic_core {

enum class TaskStatus { PENDING, PROCESSING, COMPLETED, FAILED };

inline std::string to_string(TaskStatus status) {
  switch (status) {
    case TaskStatus::PENDING: return "PENDING";
    case TaskStatus::PROCESSING: return "PROCESSING";
    case TaskStatus::COMPLETED: return "COMPLETED";
    case TaskStatus::FAILED: return "FAILED";
  }
  return "UNKNOWN";
}

inline TaskStatus task_status_from_string(const std::string &str) {
  if (str == "PENDING") return TaskStatus::PENDING;
  if (str == "PROCESSING") return TaskStatus::PROCESSING;
  if (str == "COMPLETED") return TaskStatus::COMPLETED;
  if (str == "FAILED") return TaskStatus::FAILED;
  throw std::invalid_argument("Invalid TaskStatus string: " + str);
}

struct Task {
  long long id = 0;
  std::string task_type;
  std::string file_path;
  TaskStatus status = TaskStatus::PENDING;
  int priority = 10;
  std::string error_message;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point updated_at;
};

}  // namespace magic_core


