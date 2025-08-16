#pragma once

#include <memory>

namespace magic_core {
class MetadataStore;
class TaskQueueRepo;
class OllamaClient;
class ContentExtractorFactory;
}

namespace magic_core {

class ServiceProvider {
 public:
  ServiceProvider(std::shared_ptr<MetadataStore> store,
                  std::shared_ptr<TaskQueueRepo> repo,
                  std::shared_ptr<OllamaClient> ollama,
                  std::shared_ptr<ContentExtractorFactory> factory)
      : store_(store), task_repo_(repo), ollama_client_(ollama), content_extractor_fac_(factory) {}

  // Public getters for each service
  MetadataStore& get_metadata_store() {
    return *store_;
  }
  TaskQueueRepo& get_task_queue_repo() {
    return *task_repo_;
  }
  OllamaClient& get_ollama_client() {
    return *ollama_client_;
  }
  ContentExtractorFactory& get_extractor_factory() {
    return *content_extractor_fac_;
  }

 private:
  std::shared_ptr<MetadataStore> store_;
  std::shared_ptr<TaskQueueRepo> task_repo_;
  std::shared_ptr<OllamaClient> ollama_client_;
  std::shared_ptr<ContentExtractorFactory> content_extractor_fac_;
};

}  // namespace magic_core