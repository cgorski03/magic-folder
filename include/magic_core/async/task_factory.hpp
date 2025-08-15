#include "magic_core/async/ITask.hpp"
#include "magic_core/async/process_file_task.hpp"

namespace magic_core {
ITaskPtr create_task(const TaskDTO& record) {
  if (record.task_type == "PROCESS_FILE") {
      if (!record.target_path) {
          throw std::runtime_error("PROCESS_FILE task is missing required target_path.");
      }
      return std::make_unique<ProcessFileTask>(
          record.id, record.status, record.created_at, *record.target_path);
  }

  return nullptr;
}
}