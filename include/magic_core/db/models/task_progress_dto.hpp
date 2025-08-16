#pragma once

#include <string>

namespace magic_core {

struct TaskProgressDTO {
  long long task_id;
  float progress_percent;
  std::string status_message;
  std::string updated_at;
};

}