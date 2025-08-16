#include "magic_core/async/task_factory.hpp"

#include "magic_core/db/models/task_dto.hpp"

namespace magic_core {
ITaskPtr TaskFactory::create_task(const TaskDTO& record) {
  if (record.task_type == "PROCESS_FILE") {
    if (!record.target_path) {
      throw std::runtime_error("PROCESS_FILE task is missing required target_path.");
    }
    return std::make_unique<ProcessFileTask>(record.id, record.status, record.created_at,
                                             record.updated_at, record.error_message,
                                             *record.target_path);
  }

  return nullptr;
}
}  // namespace magic_core