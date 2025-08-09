#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

#include "../../common/utilities_test.hpp"
#include "magic_core/db/task.hpp"

namespace magic_core {

class TaskQueueRepoTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    magic_tests::MetadataStoreTestBase::SetUp();
    // task_queue_repo_ is already initialized in base fixture
  }
};

TEST_F(TaskQueueRepoTest, CreateTask_BasicFunctionality) {
  std::string task_type = "PROCESS_NEW_FILE";
  std::string file_path = "/test/new_file.txt";
  int priority = 5;

  long long task_id = task_queue_repo_->create_task(task_type, file_path, priority);

  EXPECT_GT(task_id, 0);

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  ASSERT_EQ(pending_tasks.size(), 1);
  EXPECT_EQ(pending_tasks[0].id, task_id);
  EXPECT_EQ(pending_tasks[0].task_type, task_type);
  EXPECT_EQ(pending_tasks[0].file_path, file_path);
  EXPECT_EQ(pending_tasks[0].status, TaskStatus::PENDING);
  EXPECT_EQ(pending_tasks[0].priority, priority);
}

TEST_F(TaskQueueRepoTest, CreateTask_DefaultPriority) {
  std::string task_type = "PROCESS_NEW_FILE";
  std::string file_path = "/test/default_priority.txt";

  long long task_id = task_queue_repo_->create_task(task_type, file_path);
  (void)task_id;

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  ASSERT_EQ(pending_tasks.size(), 1);
  EXPECT_EQ(pending_tasks[0].priority, 10);
}

TEST_F(TaskQueueRepoTest, FetchAndClaimNextTask_BasicFunctionality) {
  long long task1_id = task_queue_repo_->create_task("PROCESS_FILE", "/test/file1.txt", 5);
  long long task2_id = task_queue_repo_->create_task("PROCESS_FILE", "/test/file2.txt", 1);
  long long task3_id = task_queue_repo_->create_task("PROCESS_FILE", "/test/file3.txt", 10);
  (void)task1_id; (void)task3_id;

  auto claimed_task = task_queue_repo_->fetch_and_claim_next_task();

  ASSERT_TRUE(claimed_task.has_value());
  EXPECT_EQ(claimed_task->id, task2_id);
  EXPECT_EQ(claimed_task->status, TaskStatus::PROCESSING);
  EXPECT_EQ(claimed_task->priority, 1);

  auto processing_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PROCESSING);
  ASSERT_EQ(processing_tasks.size(), 1);
  EXPECT_EQ(processing_tasks[0].id, task2_id);
}

TEST_F(TaskQueueRepoTest, FetchAndClaimNextTask_NoTasksAvailable) {
  auto claimed_task = task_queue_repo_->fetch_and_claim_next_task();
  EXPECT_FALSE(claimed_task.has_value());
}

TEST_F(TaskQueueRepoTest, UpdateTaskStatus_BasicFunctionality) {
  long long task_id = task_queue_repo_->create_task("PROCESS_FILE", "/test/file.txt");

  task_queue_repo_->update_task_status(task_id, TaskStatus::COMPLETED);

  auto completed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::COMPLETED);
  ASSERT_EQ(completed_tasks.size(), 1);
  EXPECT_EQ(completed_tasks[0].id, task_id);
  EXPECT_EQ(completed_tasks[0].status, TaskStatus::COMPLETED);

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  EXPECT_EQ(pending_tasks.size(), 0);
}

TEST_F(TaskQueueRepoTest, MarkTaskAsFailed_BasicFunctionality) {
  long long task_id = task_queue_repo_->create_task("PROCESS_FILE", "/test/file.txt");
  std::string error_message = "File not found";

  task_queue_repo_->mark_task_as_failed(task_id, error_message);

  auto failed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::FAILED);
  ASSERT_EQ(failed_tasks.size(), 1);
  EXPECT_EQ(failed_tasks[0].id, task_id);
  EXPECT_EQ(failed_tasks[0].status, TaskStatus::FAILED);
  EXPECT_EQ(failed_tasks[0].error_message, error_message);
}

TEST_F(TaskQueueRepoTest, GetTasksByStatus_MultipleStatuses) {
  long long pending_task1 = task_queue_repo_->create_task("PROCESS_FILE", "/test/file1.txt", 5);
  long long pending_task2 = task_queue_repo_->create_task("PROCESS_FILE", "/test/file2.txt", 1);
  long long processing_task = task_queue_repo_->create_task("PROCESS_FILE", "/test/file3.txt", 3);
  (void)pending_task1; (void)pending_task2;

  task_queue_repo_->update_task_status(processing_task, TaskStatus::PROCESSING);

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  ASSERT_EQ(pending_tasks.size(), 2);
  EXPECT_EQ(pending_tasks[0].priority, 1);
  EXPECT_EQ(pending_tasks[1].priority, 5);

  auto processing_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PROCESSING);
  ASSERT_EQ(processing_tasks.size(), 1);
  EXPECT_EQ(processing_tasks[0].id, processing_task);

  auto completed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::COMPLETED);
  EXPECT_EQ(completed_tasks.size(), 0);
}

TEST_F(TaskQueueRepoTest, ClearCompletedTasks_BasicFunctionality) {
  long long pending_task = task_queue_repo_->create_task("PROCESS_FILE", "/test/file1.txt");
  long long completed_task = task_queue_repo_->create_task("PROCESS_FILE", "/test/file2.txt");
  long long failed_task = task_queue_repo_->create_task("PROCESS_FILE", "/test/file3.txt");
  (void)pending_task;

  task_queue_repo_->update_task_status(completed_task, TaskStatus::COMPLETED);
  task_queue_repo_->mark_task_as_failed(failed_task, "Test error");

  task_queue_repo_->clear_completed_tasks(0);

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  EXPECT_EQ(pending_tasks.size(), 1);

  auto completed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::COMPLETED);
  EXPECT_EQ(completed_tasks.size(), 0);

  auto failed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::FAILED);
  EXPECT_EQ(failed_tasks.size(), 0);
}

TEST_F(TaskQueueRepoTest, TaskPriorityOrdering_CorrectOrder) {
  long long task_low = task_queue_repo_->create_task("PROCESS_FILE", "/test/low.txt", 10);
  long long task_high = task_queue_repo_->create_task("PROCESS_FILE", "/test/high.txt", 1);
  long long task_med = task_queue_repo_->create_task("PROCESS_FILE", "/test/med.txt", 5);

  auto first_task = task_queue_repo_->fetch_and_claim_next_task();
  auto second_task = task_queue_repo_->fetch_and_claim_next_task();
  auto third_task = task_queue_repo_->fetch_and_claim_next_task();

  ASSERT_TRUE(first_task.has_value());
  EXPECT_EQ(first_task->id, task_high);

  ASSERT_TRUE(second_task.has_value());
  EXPECT_EQ(second_task->id, task_med);

  ASSERT_TRUE(third_task.has_value());
  EXPECT_EQ(third_task->id, task_low);

  auto no_task = task_queue_repo_->fetch_and_claim_next_task();
  EXPECT_FALSE(no_task.has_value());
}

TEST_F(TaskQueueRepoTest, TaskTimestamps_AreSetCorrectly) {
  auto before_create = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());

  long long task_id = task_queue_repo_->create_task("PROCESS_FILE", "/test/file.txt");
  (void)task_id;

  auto after_create = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()) + std::chrono::seconds(1);

  auto tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  ASSERT_EQ(tasks.size(), 1);

  auto &task = tasks[0];
  EXPECT_GE(task.created_at, before_create);
  EXPECT_LE(task.created_at, after_create);
  EXPECT_GE(task.updated_at, before_create);
  EXPECT_LE(task.updated_at, after_create);
  EXPECT_EQ(task.created_at, task.updated_at);
}

TEST_F(TaskQueueRepoTest, TaskClaiming_AtomicOperation) {
  long long task_id = task_queue_repo_->create_task("PROCESS_FILE", "/test/file.txt");

  auto task1 = task_queue_repo_->fetch_and_claim_next_task();
  auto task2 = task_queue_repo_->fetch_and_claim_next_task();

  EXPECT_TRUE(task1.has_value());
  EXPECT_FALSE(task2.has_value());

  if (task1.has_value()) {
    EXPECT_EQ(task1->id, task_id);
    EXPECT_EQ(task1->status, TaskStatus::PROCESSING);
  }
}

}  // namespace magic_core
