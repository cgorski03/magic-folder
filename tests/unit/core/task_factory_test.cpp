#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <optional>

#include "magic_core/async/task_factory.hpp"
#include "magic_core/async/process_file_task.hpp"
#include "magic_core/db/models/task_dto.hpp"

namespace magic_tests {

using namespace magic_core;

class TaskFactoryTest : public ::testing::Test {
 protected:
  TaskDTO create_test_task_dto(const std::string& task_type, 
                               const std::optional<std::string>& target_path = std::nullopt) {
    TaskDTO task;
    task.id = 123;
    task.task_type = task_type;
    task.status = TaskStatus::PENDING;
    task.priority = 5;
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = std::chrono::system_clock::now();
    task.target_path = target_path;
    task.error_message = std::nullopt;
    return task;
  }
};

TEST_F(TaskFactoryTest, CreateTask_ProcessFileTask_ValidPath) {
  // Arrange
  TaskDTO task_dto = create_test_task_dto("PROCESS_FILE", "/test/file.txt");
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert
  ASSERT_NE(task, nullptr);
  EXPECT_STREQ(task->get_type(), "PROCESS_FILE");
  EXPECT_EQ(task->get_id(), 123);
  EXPECT_EQ(task->get_status(), TaskStatus::PENDING);
  
  // Verify it's actually a ProcessFileTask
  auto* process_task = dynamic_cast<ProcessFileTask*>(task.get());
  ASSERT_NE(process_task, nullptr);
  EXPECT_EQ(process_task->get_file_path(), "/test/file.txt");
}

TEST_F(TaskFactoryTest, CreateTask_ProcessFileTask_MissingTargetPath) {
  // Arrange
  TaskDTO task_dto = create_test_task_dto("PROCESS_FILE", std::nullopt);
  
  // Act & Assert
  EXPECT_THROW({
    TaskFactory::create_task(task_dto);
  }, std::runtime_error);
}

TEST_F(TaskFactoryTest, CreateTask_ProcessFileTask_EmptyTargetPath) {
  // Arrange
  TaskDTO task_dto = create_test_task_dto("PROCESS_FILE", "");
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert
  ASSERT_NE(task, nullptr);
  auto* process_task = dynamic_cast<ProcessFileTask*>(task.get());
  ASSERT_NE(process_task, nullptr);
  EXPECT_EQ(process_task->get_file_path(), "");
}

TEST_F(TaskFactoryTest, CreateTask_UnknownTaskType_ReturnsNull) {
  // Arrange
  TaskDTO task_dto = create_test_task_dto("UNKNOWN_TASK");
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert
  EXPECT_EQ(task, nullptr);
}

TEST_F(TaskFactoryTest, CreateTask_EmptyTaskType_ReturnsNull) {
  // Arrange
  TaskDTO task_dto = create_test_task_dto("");
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert
  EXPECT_EQ(task, nullptr);
}

TEST_F(TaskFactoryTest, CreateTask_Casesensitivity) {
  // Arrange - Test case sensitivity
  TaskDTO task_dto = create_test_task_dto("process_file", "/test/file.txt");
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert - Should return null for wrong case
  EXPECT_EQ(task, nullptr);
}

TEST_F(TaskFactoryTest, CreateTask_ProcessFileTask_LongFilePath) {
  // Arrange
  std::string long_path = "/very/long/path/to/some/deeply/nested/directory/structure/with/many/subdirectories/file.txt";
  TaskDTO task_dto = create_test_task_dto("PROCESS_FILE", long_path);
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert
  ASSERT_NE(task, nullptr);
  auto* process_task = dynamic_cast<ProcessFileTask*>(task.get());
  ASSERT_NE(process_task, nullptr);
  EXPECT_EQ(process_task->get_file_path(), long_path);
}

TEST_F(TaskFactoryTest, CreateTask_ProcessFileTask_SpecialCharactersInPath) {
  // Arrange
  std::string special_path = "/test/file with spaces & special chars!@#$%.txt";
  TaskDTO task_dto = create_test_task_dto("PROCESS_FILE", special_path);
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert
  ASSERT_NE(task, nullptr);
  auto* process_task = dynamic_cast<ProcessFileTask*>(task.get());
  ASSERT_NE(process_task, nullptr);
  EXPECT_EQ(process_task->get_file_path(), special_path);
}

TEST_F(TaskFactoryTest, CreateTask_ProcessFileTask_WithErrorMessage) {
  // Arrange
  TaskDTO task_dto = create_test_task_dto("PROCESS_FILE", "/test/file.txt");
  task_dto.error_message = "Previous error occurred";
  task_dto.status = TaskStatus::FAILED;
  
  // Act
  ITaskPtr task = TaskFactory::create_task(task_dto);
  
  // Assert
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->get_status(), TaskStatus::FAILED);
  EXPECT_EQ(task->get_id(), 123);
}

TEST_F(TaskFactoryTest, CreateTask_ProcessFileTask_DifferentStatuses) {
  // Test different task statuses
  std::vector<TaskStatus> statuses = {
    TaskStatus::PENDING, 
    TaskStatus::PROCESSING, 
    TaskStatus::COMPLETED, 
    TaskStatus::FAILED
  };
  
  for (auto status : statuses) {
    // Arrange
    TaskDTO task_dto = create_test_task_dto("PROCESS_FILE", "/test/file.txt");
    task_dto.status = status;
    
    // Act
    ITaskPtr task = TaskFactory::create_task(task_dto);
    
    // Assert
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->get_status(), status);
  }
}

} // namespace magic_tests


