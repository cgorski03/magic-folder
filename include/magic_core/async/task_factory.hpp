#include "magic_core/async/ITask.hpp"
#include "magic_core/async/process_file_task.hpp"

namespace magic_core {
class TaskFactory {
 public:
  static ITaskPtr create_task(const TaskDTO& record);
};
}  // namespace magic_core