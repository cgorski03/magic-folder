#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <functional>
#include "magic_core/db/models/task_dto.hpp"

namespace magic_core {
class ServiceProvider;
}
using ProgressUpdater = std::function<void(float, const std::string&)>;

namespace magic_core {
class ITask {
 public:
  ITask(long long id,
        TaskStatus status,
        std::chrono::system_clock::time_point created_at,
        std::chrono::system_clock::time_point updated_at,
        std::optional<std::string> error_message)
      : id_(id),
        status_(status),
        created_at_(created_at),
        updated_at_(updated_at),
        error_message_(error_message) {}

  virtual ~ITask() = default;

  virtual void execute(ServiceProvider& services, const ProgressUpdater& on_progress) = 0;

  virtual const char* get_type() const = 0;

  long long get_id() const {
    return id_;
  }
  TaskStatus get_status() const {
    return status_;
  }

 protected:
  long long id_;
  TaskStatus status_;
  std::chrono::system_clock::time_point created_at_;
  std::chrono::system_clock::time_point updated_at_;
  std::optional<std::string> error_message_;
};

using ITaskPtr = std::unique_ptr<ITask>;
}  // namespace magic_core