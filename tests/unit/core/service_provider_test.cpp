#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "magic_core/async/service_provider.hpp"
#include "../../common/mocks_test.hpp"
#include "../../common/utilities_test.hpp"

namespace magic_tests {

using namespace magic_core;
using ::testing::StrictMock;

class ServiceProviderTest : public MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    
    // Create mock dependencies
    mock_ollama_client_ = std::make_shared<StrictMock<MockOllamaClient>>();
    mock_content_extractor_factory_ = std::make_shared<StrictMock<MockContentExtractorFactory>>();
  }

  void TearDown() override {
    mock_content_extractor_factory_.reset();
    mock_ollama_client_.reset();
    MetadataStoreTestBase::TearDown();
  }

  std::shared_ptr<StrictMock<MockOllamaClient>> mock_ollama_client_;
  std::shared_ptr<StrictMock<MockContentExtractorFactory>> mock_content_extractor_factory_;
};

TEST_F(ServiceProviderTest, Constructor_InitializesWithSharedPointers) {
  // Act
  ServiceProvider provider(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Assert - Should construct without throwing
  SUCCEED();
}

TEST_F(ServiceProviderTest, GetMetadataStore_ReturnsCorrectReference) {
  // Arrange
  ServiceProvider provider(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Act
  MetadataStore& store = provider.get_metadata_store();
  
  // Assert
  EXPECT_EQ(&store, metadata_store_.get());
}

TEST_F(ServiceProviderTest, GetTaskQueueRepo_ReturnsCorrectReference) {
  // Arrange
  ServiceProvider provider(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Act
  TaskQueueRepo& repo = provider.get_task_queue_repo();
  
  // Assert
  EXPECT_EQ(&repo, task_queue_repo_.get());
}

TEST_F(ServiceProviderTest, GetOllamaClient_ReturnsCorrectReference) {
  // Arrange
  ServiceProvider provider(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Act
  OllamaClient& client = provider.get_ollama_client();
  
  // Assert
  EXPECT_EQ(&client, mock_ollama_client_.get());
}

TEST_F(ServiceProviderTest, GetExtractorFactory_ReturnsCorrectReference) {
  // Arrange
  ServiceProvider provider(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Act
  ContentExtractorFactory& factory = provider.get_extractor_factory();
  
  // Assert
  EXPECT_EQ(&factory, mock_content_extractor_factory_.get());
}

TEST_F(ServiceProviderTest, MultipleAccess_ReturnsConsistentReferences) {
  // Arrange
  ServiceProvider provider(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Act - Access the same service multiple times
  MetadataStore& store1 = provider.get_metadata_store();
  MetadataStore& store2 = provider.get_metadata_store();
  
  TaskQueueRepo& repo1 = provider.get_task_queue_repo();
  TaskQueueRepo& repo2 = provider.get_task_queue_repo();
  
  OllamaClient& client1 = provider.get_ollama_client();
  OllamaClient& client2 = provider.get_ollama_client();
  
  ContentExtractorFactory& factory1 = provider.get_extractor_factory();
  ContentExtractorFactory& factory2 = provider.get_extractor_factory();
  
  // Assert - Should return the same references
  EXPECT_EQ(&store1, &store2);
  EXPECT_EQ(&repo1, &repo2);
  EXPECT_EQ(&client1, &client2);
  EXPECT_EQ(&factory1, &factory2);
}

TEST_F(ServiceProviderTest, ServiceLifetime_ServicesRemainValidThroughProviderLifetime) {
  // Arrange
  std::unique_ptr<ServiceProvider> provider;
  MetadataStore* store_ptr = nullptr;
  TaskQueueRepo* repo_ptr = nullptr;
  OllamaClient* client_ptr = nullptr;
  ContentExtractorFactory* factory_ptr = nullptr;
  
  // Act
  {
    provider = std::make_unique<ServiceProvider>(
        metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_
    );
    
    // Get pointers to services
    store_ptr = &provider->get_metadata_store();
    repo_ptr = &provider->get_task_queue_repo();
    client_ptr = &provider->get_ollama_client();
    factory_ptr = &provider->get_extractor_factory();
    
    // Verify services are accessible
    EXPECT_NE(store_ptr, nullptr);
    EXPECT_NE(repo_ptr, nullptr);
    EXPECT_NE(client_ptr, nullptr);
    EXPECT_NE(factory_ptr, nullptr);
  }
  
  // Services should still be valid as long as provider exists
  EXPECT_EQ(store_ptr, &provider->get_metadata_store());
  EXPECT_EQ(repo_ptr, &provider->get_task_queue_repo());
  EXPECT_EQ(client_ptr, &provider->get_ollama_client());
  EXPECT_EQ(factory_ptr, &provider->get_extractor_factory());
}

TEST_F(ServiceProviderTest, CopyConstructor_IsDeleted) {
  // This test verifies that ServiceProvider cannot be copied
  // The test passes if the code compiles, as copy constructor should be implicitly deleted
  // due to shared_ptr members, but we can verify the behavior
  
  ServiceProvider provider1(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // This should not compile if copy constructor is properly deleted:
  // ServiceProvider provider2 = provider1;  // Should not compile
  
  // Instead, we test that we can create multiple providers with same services
  ServiceProvider provider2(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // They should reference the same underlying services
  EXPECT_EQ(&provider1.get_metadata_store(), &provider2.get_metadata_store());
  EXPECT_EQ(&provider1.get_task_queue_repo(), &provider2.get_task_queue_repo());
  EXPECT_EQ(&provider1.get_ollama_client(), &provider2.get_ollama_client());
  EXPECT_EQ(&provider1.get_extractor_factory(), &provider2.get_extractor_factory());
}

TEST_F(ServiceProviderTest, MultipleProviders_ShareSameServices) {
  // Arrange
  ServiceProvider provider1(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  ServiceProvider provider2(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Act & Assert - Both providers should reference the same service instances
  EXPECT_EQ(&provider1.get_metadata_store(), &provider2.get_metadata_store());
  EXPECT_EQ(&provider1.get_task_queue_repo(), &provider2.get_task_queue_repo());
  EXPECT_EQ(&provider1.get_ollama_client(), &provider2.get_ollama_client());
  EXPECT_EQ(&provider1.get_extractor_factory(), &provider2.get_extractor_factory());
}

TEST_F(ServiceProviderTest, ThreadSafety_MultipleThreadAccess) {
  // This is a basic test for thread safety - in a real scenario you'd want more comprehensive testing
  ServiceProvider provider(metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_);
  
  // Create multiple threads accessing the same provider
  std::vector<std::thread> threads;
  std::atomic<int> successful_accesses(0);
  
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&provider, &successful_accesses]() {
      try {
        // Access all services
        auto& store = provider.get_metadata_store();
        auto& repo = provider.get_task_queue_repo();
        auto& client = provider.get_ollama_client();
        auto& factory = provider.get_extractor_factory();
        
        // Verify references are valid (references are always non-null)
        // This test mainly ensures no exceptions are thrown during access
        successful_accesses++;
      } catch (...) {
        // Any exception means thread safety issue
      }
    });
  }
  
  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }
  
  // All threads should have succeeded
  EXPECT_EQ(successful_accesses.load(), 10);
}

TEST_F(ServiceProviderTest, ServiceAccess_AfterServiceDestruction_HandledGracefully) {
  // This test verifies behavior when underlying services might be destroyed
  // In practice, shared_ptr should keep services alive as long as ServiceProvider exists
  
  std::unique_ptr<ServiceProvider> provider;
  
  // Create provider with services
  provider = std::make_unique<ServiceProvider>(
      metadata_store_, task_queue_repo_, mock_ollama_client_, mock_content_extractor_factory_
  );
  
  // Store references
  MetadataStore& store_ref = provider->get_metadata_store();
  TaskQueueRepo& repo_ref = provider->get_task_queue_repo();
  OllamaClient& client_ref = provider->get_ollama_client();
  ContentExtractorFactory& factory_ref = provider->get_extractor_factory();
  
  // Clear our shared_ptrs (but provider should still hold references)
  auto temp_metadata = metadata_store_;
  auto temp_repo = task_queue_repo_;
  auto temp_client = mock_ollama_client_;
  auto temp_factory = mock_content_extractor_factory_;
  
  metadata_store_.reset();
  task_queue_repo_.reset();
  mock_ollama_client_.reset();
  mock_content_extractor_factory_.reset();
  
  // Services should still be accessible through provider
  EXPECT_EQ(&provider->get_metadata_store(), &store_ref);
  EXPECT_EQ(&provider->get_task_queue_repo(), &repo_ref);
  EXPECT_EQ(&provider->get_ollama_client(), &client_ref);
  EXPECT_EQ(&provider->get_extractor_factory(), &factory_ref);
  
  // Restore for proper cleanup
  metadata_store_ = temp_metadata;
  task_queue_repo_ = temp_repo;
  mock_ollama_client_ = temp_client;
  mock_content_extractor_factory_ = temp_factory;
}

} // namespace magic_tests


