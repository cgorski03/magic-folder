#include "magic_core/db/task_queue_repo.hpp"

#include <sqlite_modern_cpp.h>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "magic_core/db/pooled_connection.hpp"
#include "magic_core/db/sqlite_error_utils.hpp"
#include "magic_core/db/task.hpp"
#include "magic_core/db/transaction.hpp"

namespace magic_core {

static std::string format_time(const std::chrono::system_clock::time_point& tp) {
  auto time_t = std::chrono::system_clock::to_time_t(tp);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

static std::chrono::system_clock::time_point parse_time(const std::string& time_str) {
  std::tm tm_struct = {};
  std::stringstream ss(time_str);
  ss >> std::get_time(&tm_struct, "%Y-%m-%d %H:%M:%S");
  return std::chrono::system_clock::from_time_t(timegm(&tm_struct));
}

TaskQueueRepo::TaskQueueRepo(DatabaseManager& db_manager) : db_manager_(db_manager) {}

std::string TaskQueueRepo::time_point_to_string(const std::chrono::system_clock::time_point& tp) {
  return format_time(tp);
}
std::chrono::system_clock::time_point TaskQueueRepo::string_to_time_point(
    const std::string& time_str) {
  return parse_time(time_str);
}

long long TaskQueueRepo::create_task(const std::string& task_type,
                                     const std::string& file_path,
                                     int priority) {
  PooledConnection conn(db_manager_);
  try {
    auto now = std::chrono::system_clock::now();
    std::string created_at_str = time_point_to_string(now);
    std::string updated_at_str = created_at_str;
    *conn << "INSERT INTO task_queue (task_type, file_path, priority, created_at, updated_at) VALUES "
           "(?,?,?,?,?)"
        << task_type << file_path << priority << created_at_str << updated_at_str;
    return static_cast<long long>(conn->last_insert_rowid());
  } catch (const sqlite::sqlite_exception& e) {
    throw TaskQueueRepoError(format_db_error("create_task", e));
  }
}

std::optional<Task> TaskQueueRepo::fetch_and_claim_next_task() {
  std::optional<Task> result;
  try {
    PooledConnection conn(db_manager_);
    Transaction tx(*conn, true);
    std::string pending_status = to_string(TaskStatus::PENDING);
    *conn << "SELECT id, task_type, file_path, status, priority, error_message, created_at, "
           "updated_at FROM task_queue WHERE status = ? ORDER BY priority ASC, created_at ASC "
           "LIMIT 1"
        << pending_status >>
        [&](long long id, std::string task_type, std::string file_path, std::string status_db,
            int priority, std::optional<std::string> error_message, std::string created_at,
            std::string updated_at) {
          Task task;
          task.id = id;
          task.task_type = task_type;
          task.file_path = file_path;
          task.status = task_status_from_string(status_db);
          task.priority = priority;
          if (error_message)
            task.error_message = *error_message;
          task.created_at = string_to_time_point(created_at);
          task.updated_at = string_to_time_point(updated_at);
          result = std::move(task);
        };

    if (result) {
      auto now = std::chrono::system_clock::now();
      std::string updated_at_str = time_point_to_string(now);
      std::string processing_status = to_string(TaskStatus::PROCESSING);
      *conn << "UPDATE task_queue SET status = ?, updated_at = ? WHERE id = ?" << processing_status
          << updated_at_str << result->id;
      result->status = TaskStatus::PROCESSING;
      result->updated_at = now;
    }
    tx.commit();
    return result;
  } catch (const sqlite::sqlite_exception& e) {
    throw TaskQueueRepoError(format_db_error("fetch_and_claim_next_task", e));
  }
}

void TaskQueueRepo::update_task_status(long long task_id, TaskStatus new_status) {
  try {
    PooledConnection conn(db_manager_);
    auto now = std::chrono::system_clock::now();
    std::string updated_at_str = time_point_to_string(now);
    std::string status_str = to_string(new_status);
    *conn << "UPDATE task_queue SET status = ?, updated_at = ? WHERE id = ?" << status_str
        << updated_at_str << task_id;
  } catch (const sqlite::sqlite_exception& e) {
    throw TaskQueueRepoError(format_db_error("update_task_status", e));
  }
}

void TaskQueueRepo::mark_task_as_failed(long long task_id, const std::string& error_message) {
  try {
    PooledConnection conn(db_manager_);
    auto now = std::chrono::system_clock::now();
    std::string updated_at_str = time_point_to_string(now);
    std::string failed_status = to_string(TaskStatus::FAILED);
    *conn << "UPDATE task_queue SET status = ?, error_message = ?, updated_at = ? WHERE id = ?"
        << failed_status << error_message << updated_at_str << task_id;
  } catch (const sqlite::sqlite_exception& e) {
    throw TaskQueueRepoError(format_db_error("mark_task_as_failed", e));
  }
}

std::vector<Task> TaskQueueRepo::get_tasks_by_status(TaskStatus status) {
  try {
    PooledConnection conn(db_manager_);
    std::vector<Task> tasks;
    std::string status_str = to_string(status);
    *conn << "SELECT id, task_type, file_path, status, priority, error_message, created_at, "
           "updated_at FROM task_queue WHERE status = ? ORDER BY priority ASC, created_at ASC"
        << status_str >>
        [&](long long id, std::string task_type, std::string file_path, std::string status_db,
            int priority, std::optional<std::string> error_message, std::string created_at,
            std::string updated_at) {
          Task task;
          task.id = id;
          task.task_type = task_type;
          task.file_path = file_path;
          task.status = task_status_from_string(status_db);
          task.priority = priority;
          if (error_message)
            task.error_message = *error_message;
          task.created_at = string_to_time_point(created_at);
          task.updated_at = string_to_time_point(updated_at);
          tasks.push_back(std::move(task));
        };
    return tasks;
  } catch (const sqlite::sqlite_exception& e) {
    throw TaskQueueRepoError(format_db_error("get_tasks_by_status", e));
  }
}

void TaskQueueRepo::clear_completed_tasks(int older_than_days) {
  try {
    PooledConnection conn(db_manager_);
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * older_than_days);
    std::string cutoff_str = time_point_to_string(cutoff_time);
    std::string completed_status = to_string(TaskStatus::COMPLETED);
    std::string failed_status = to_string(TaskStatus::FAILED);
    *conn << "DELETE FROM task_queue WHERE status IN (?, ?) AND updated_at <= ?" << completed_status
        << failed_status << cutoff_str;
  } catch (const sqlite::sqlite_exception& e) {
    throw TaskQueueRepoError(format_db_error("clear_completed_tasks", e));
  }
}

}  // namespace magic_core
