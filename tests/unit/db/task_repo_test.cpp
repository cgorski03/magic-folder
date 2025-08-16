#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../../common/utilities_test.hpp"
#include "magic_core/db/models/task_dto.hpp"

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

  long long task_id = task_queue_repo_->create_file_process_task(task_type, file_path, priority);

  EXPECT_GT(task_id, 0);

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  ASSERT_EQ(pending_tasks.size(), 1);
  EXPECT_EQ(pending_tasks[0].id, task_id);
  EXPECT_EQ(pending_tasks[0].task_type, task_type);
  EXPECT_EQ(pending_tasks[0].target_path, file_path);
  EXPECT_EQ(pending_tasks[0].status, TaskStatus::PENDING);
  EXPECT_EQ(pending_tasks[0].priority, priority);
}

TEST_F(TaskQueueRepoTest, CreateTask_DefaultPriority) {
  std::string task_type = "PROCESS_NEW_FILE";
  std::string file_path = "/test/default_priority.txt";

  long long task_id = task_queue_repo_->create_file_process_task(task_type, file_path);
  (void)task_id;

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  ASSERT_EQ(pending_tasks.size(), 1);
  EXPECT_EQ(pending_tasks[0].priority, 10);
}

TEST_F(TaskQueueRepoTest, FetchAndClaimNextTask_BasicFunctionality) {
  long long task1_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file1.txt", 5);
  long long task2_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file2.txt", 1);
  long long task3_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file3.txt", 10);
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
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");

  task_queue_repo_->update_task_status(task_id, TaskStatus::COMPLETED);

  auto completed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::COMPLETED);
  ASSERT_EQ(completed_tasks.size(), 1);
  EXPECT_EQ(completed_tasks[0].id, task_id);
  EXPECT_EQ(completed_tasks[0].status, TaskStatus::COMPLETED);

  auto pending_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::PENDING);
  EXPECT_EQ(pending_tasks.size(), 0);
}

TEST_F(TaskQueueRepoTest, MarkTaskAsFailed_BasicFunctionality) {
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  std::string error_message = "File not found";

  task_queue_repo_->mark_task_as_failed(task_id, error_message);

  auto failed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::FAILED);
  ASSERT_EQ(failed_tasks.size(), 1);
  EXPECT_EQ(failed_tasks[0].id, task_id);
  EXPECT_EQ(failed_tasks[0].status, TaskStatus::FAILED);
  EXPECT_EQ(failed_tasks[0].error_message, error_message);
}

TEST_F(TaskQueueRepoTest, GetTasksByStatus_MultipleStatuses) {
  long long pending_task1 = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file1.txt", 5);
  long long pending_task2 = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file2.txt", 1);
  long long processing_task = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file3.txt", 3);
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
  long long pending_task = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file1.txt");
  long long completed_task = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file2.txt");
  long long failed_task = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file3.txt");
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
  long long task_low = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/low.txt", 10);
  long long task_high = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/high.txt", 1);
  long long task_med = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/med.txt", 5);

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

  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
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
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");

  // Simulate concurrent claims using multiple threads
  std::optional<TaskDTO> t1;
  std::optional<TaskDTO> t2;
  std::thread th1([&]{ t1 = task_queue_repo_->fetch_and_claim_next_task(); });
  std::thread th2([&]{ t2 = task_queue_repo_->fetch_and_claim_next_task(); });
  th1.join();
  th2.join();

  // Exactly one thread should get the task
  EXPECT_NE(t1.has_value(), t2.has_value());
  const std::optional<TaskDTO>& claimed = t1.has_value() ? t1 : t2;
  ASSERT_TRUE(claimed.has_value());
  EXPECT_EQ(claimed->id, task_id);
  EXPECT_EQ(claimed->status, TaskStatus::PROCESSING);
}

// ============================================================================
// Task Progress Tests
// ============================================================================

TEST_F(TaskQueueRepoTest, UpsertTaskProgress_BasicFunctionality) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  float progress_percent = 25.5f;
  std::string status_message = "Processing chunks...";
  
  // Act
  EXPECT_NO_THROW({
    task_queue_repo_->upsert_task_progress(task_id, progress_percent, status_message);
  });
  
  // Assert
  auto progress = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress.has_value());
  EXPECT_EQ(progress->task_id, task_id);
  EXPECT_FLOAT_EQ(progress->progress_percent, progress_percent);
  EXPECT_EQ(progress->status_message, status_message);
  EXPECT_FALSE(progress->updated_at.empty());
}

TEST_F(TaskQueueRepoTest, UpsertTaskProgress_UpdateExistingProgress) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  
  // Act - Insert initial progress
  task_queue_repo_->upsert_task_progress(task_id, 10.0f, "Starting...");
  
  // Act - Update progress
  task_queue_repo_->upsert_task_progress(task_id, 50.0f, "Halfway done...");
  
  // Assert - Should have updated, not duplicated
  auto progress = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress.has_value());
  EXPECT_EQ(progress->task_id, task_id);
  EXPECT_FLOAT_EQ(progress->progress_percent, 50.0f);
  EXPECT_EQ(progress->status_message, "Halfway done...");
}

TEST_F(TaskQueueRepoTest, UpsertTaskProgress_MultipleProgressUpdates) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  
  // Act - Multiple sequential updates
  task_queue_repo_->upsert_task_progress(task_id, 0.0f, "Starting");
  task_queue_repo_->upsert_task_progress(task_id, 25.0f, "Quarter done");
  task_queue_repo_->upsert_task_progress(task_id, 75.0f, "Almost finished");
  task_queue_repo_->upsert_task_progress(task_id, 100.0f, "Complete");
  
  // Assert - Should only have the latest progress
  auto progress = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress.has_value());
  EXPECT_EQ(progress->task_id, task_id);
  EXPECT_FLOAT_EQ(progress->progress_percent, 100.0f);
  EXPECT_EQ(progress->status_message, "Complete");
}

TEST_F(TaskQueueRepoTest, GetTaskProgress_NonExistentTask_ReturnsEmpty) {
  // Arrange
  long long non_existent_task_id = 99999;
  
  // Act
  auto progress = task_queue_repo_->get_task_progress(non_existent_task_id);
  
  // Assert
  EXPECT_FALSE(progress.has_value());
}

TEST_F(TaskQueueRepoTest, UpsertTaskProgress_EdgeCaseValues) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  
  // Test minimum values
  task_queue_repo_->upsert_task_progress(task_id, 0.0f, "");
  auto progress_min = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress_min.has_value());
  EXPECT_FLOAT_EQ(progress_min->progress_percent, 0.0f);
  EXPECT_EQ(progress_min->status_message, "");
  
  // Test maximum values
  task_queue_repo_->upsert_task_progress(task_id, 100.0f, "Very long status message with lots of details about what is happening during this processing step");
  auto progress_max = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress_max.has_value());
  EXPECT_FLOAT_EQ(progress_max->progress_percent, 100.0f);
  EXPECT_EQ(progress_max->status_message, "Very long status message with lots of details about what is happening during this processing step");
  
  // Test negative values (should still work)
  task_queue_repo_->upsert_task_progress(task_id, -5.0f, "Error state");
  auto progress_neg = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress_neg.has_value());
  EXPECT_FLOAT_EQ(progress_neg->progress_percent, -5.0f);
  EXPECT_EQ(progress_neg->status_message, "Error state");
  
  // Test over 100% (should still work)
  task_queue_repo_->upsert_task_progress(task_id, 150.0f, "Over completion");
  auto progress_over = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress_over.has_value());
  EXPECT_FLOAT_EQ(progress_over->progress_percent, 150.0f);
  EXPECT_EQ(progress_over->status_message, "Over completion");
}

TEST_F(TaskQueueRepoTest, UpsertTaskProgress_SpecialCharacters) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  std::string special_message = "Processing file with unicode: 测试 🚀 and symbols: !@#$%^&*()";
  
  // Act
  task_queue_repo_->upsert_task_progress(task_id, 42.5f, special_message);
  
  // Assert
  auto progress = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress.has_value());
  EXPECT_EQ(progress->status_message, special_message);
}

TEST_F(TaskQueueRepoTest, TaskProgress_IndependentPerTask) {
  // Arrange
  long long task1_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file1.txt");
  long long task2_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file2.txt");
  
  // Act - Set different progress for each task
  task_queue_repo_->upsert_task_progress(task1_id, 30.0f, "Task 1 progress");
  task_queue_repo_->upsert_task_progress(task2_id, 70.0f, "Task 2 progress");
  
  // Assert - Each task should have its own progress
  auto progress1 = task_queue_repo_->get_task_progress(task1_id);
  auto progress2 = task_queue_repo_->get_task_progress(task2_id);
  
  ASSERT_TRUE(progress1.has_value());
  ASSERT_TRUE(progress2.has_value());
  
  EXPECT_EQ(progress1->task_id, task1_id);
  EXPECT_FLOAT_EQ(progress1->progress_percent, 30.0f);
  EXPECT_EQ(progress1->status_message, "Task 1 progress");
  
  EXPECT_EQ(progress2->task_id, task2_id);
  EXPECT_FLOAT_EQ(progress2->progress_percent, 70.0f);
  EXPECT_EQ(progress2->status_message, "Task 2 progress");
}

TEST_F(TaskQueueRepoTest, TaskProgress_TimestampUpdates) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  
  // Act - First update
  task_queue_repo_->upsert_task_progress(task_id, 25.0f, "First update");
  auto progress1 = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress1.has_value());
  std::string first_timestamp = progress1->updated_at;
  
  // Small delay to ensure timestamp difference (SQLite timestamps are second precision)
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  // Act - Second update
  task_queue_repo_->upsert_task_progress(task_id, 50.0f, "Second update");
  auto progress2 = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress2.has_value());
  
  // Assert - Timestamp should be updated
  EXPECT_NE(progress2->updated_at, first_timestamp);
  EXPECT_FLOAT_EQ(progress2->progress_percent, 50.0f);
  EXPECT_EQ(progress2->status_message, "Second update");
}

TEST_F(TaskQueueRepoTest, TaskProgress_ConcurrentUpdates) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  
  // Act - Concurrent progress updates
  std::thread t1([&]() {
    task_queue_repo_->upsert_task_progress(task_id, 25.0f, "Thread 1 update");
  });
  
  std::thread t2([&]() {
    task_queue_repo_->upsert_task_progress(task_id, 75.0f, "Thread 2 update");
  });
  
  t1.join();
  t2.join();
  
  // Assert - Should have one of the updates (database handles concurrency)
  auto progress = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(progress.has_value());
  EXPECT_EQ(progress->task_id, task_id);
  // Progress should be one of the two values
  EXPECT_TRUE(progress->progress_percent == 25.0f || progress->progress_percent == 75.0f);
  EXPECT_TRUE(progress->status_message == "Thread 1 update" || progress->status_message == "Thread 2 update");
}

TEST_F(TaskQueueRepoTest, TaskProgress_IntegrationWithTaskLifecycle) {
  // Arrange
  long long task_id = task_queue_repo_->create_file_process_task("PROCESS_FILE", "/test/file.txt");
  
  // Act & Assert - Test progress through task lifecycle
  
  // 1. Task created, no progress yet
  auto initial_progress = task_queue_repo_->get_task_progress(task_id);
  EXPECT_FALSE(initial_progress.has_value());
  
  // 2. Claim task and start progress
  auto claimed_task = task_queue_repo_->fetch_and_claim_next_task();
  ASSERT_TRUE(claimed_task.has_value());
  EXPECT_EQ(claimed_task->status, TaskStatus::PROCESSING);
  
  task_queue_repo_->upsert_task_progress(task_id, 10.0f, "Starting processing");
  auto start_progress = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(start_progress.has_value());
  EXPECT_FLOAT_EQ(start_progress->progress_percent, 10.0f);
  
  // 3. Update progress during processing
  task_queue_repo_->upsert_task_progress(task_id, 50.0f, "Halfway done");
  task_queue_repo_->upsert_task_progress(task_id, 90.0f, "Almost finished");
  
  // 4. Complete task
  task_queue_repo_->update_task_status(task_id, TaskStatus::COMPLETED);
  task_queue_repo_->upsert_task_progress(task_id, 100.0f, "Task completed");
  
  // 5. Verify final state
  auto final_progress = task_queue_repo_->get_task_progress(task_id);
  ASSERT_TRUE(final_progress.has_value());
  EXPECT_FLOAT_EQ(final_progress->progress_percent, 100.0f);
  EXPECT_EQ(final_progress->status_message, "Task completed");
  
  auto completed_tasks = task_queue_repo_->get_tasks_by_status(TaskStatus::COMPLETED);
  ASSERT_EQ(completed_tasks.size(), 1);
  EXPECT_EQ(completed_tasks[0].id, task_id);
}

}  // namespace magic_core
