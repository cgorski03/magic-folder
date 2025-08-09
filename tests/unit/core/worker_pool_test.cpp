#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <thread>

#include "magic_core/async/worker_pool.hpp"
#include "mocks_test.hpp"
#include "utilities_test.hpp"

namespace magic_tests {

using namespace magic_core;
using namespace magic_core::async;
using ::testing::StrictMock;

class WorkerPoolTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    magic_tests::MetadataStoreTestBase::SetUp();
    mock_ollama_client_ = std::make_unique<StrictMock<MockOllamaClient>>();
    mock_content_extractor_factory_ = std::make_unique<StrictMock<MockContentExtractorFactory>>();
  }

  void TearDown() override {
    mock_content_extractor_factory_.reset();
    mock_ollama_client_.reset();
    magic_tests::MetadataStoreTestBase::TearDown();
  }

  std::unique_ptr<StrictMock<MockOllamaClient>> mock_ollama_client_;
  std::unique_ptr<StrictMock<MockContentExtractorFactory>> mock_content_extractor_factory_;
};

TEST_F(WorkerPoolTest, ConstructorThrowsOnZeroThreads) {
  EXPECT_THROW({
    WorkerPool pool(0, *metadata_store_, *mock_ollama_client_, *mock_content_extractor_factory_);
  }, std::invalid_argument);
}

TEST_F(WorkerPoolTest, StopWithoutStartIsNoOp) {
  EXPECT_NO_THROW({
    WorkerPool pool(1, *metadata_store_, *mock_ollama_client_, *mock_content_extractor_factory_);
    pool.stop();
  });
}

TEST_F(WorkerPoolTest, StartThenStopLifecycle_NoTasks) {
  EXPECT_NO_THROW({
    WorkerPool pool(1, *metadata_store_, *mock_ollama_client_, *mock_content_extractor_factory_);
    pool.start();
    // Give the worker thread a brief moment to enter its loop
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    pool.stop();
    // Destructor will join worker thread
  });
}

TEST_F(WorkerPoolTest, StartTwiceShowsWarningAndNoThrow) {
  EXPECT_NO_THROW({
    WorkerPool pool(1, *metadata_store_, *mock_ollama_client_, *mock_content_extractor_factory_);
    pool.start();
    // Second call should be a no-op with a warning, not an exception
    pool.start();
    pool.stop();
  });
}

}  // namespace magic_tests


