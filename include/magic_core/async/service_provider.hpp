#pragma once

namespace magic_core {
class MetadataStore;
class TaskQueueRepo;
class OllamaClient;
class ContentExtractorFactory;
}

namespace magic_core {

class ServiceProvider {
 public:
  ServiceProvider(MetadataStore& store,
                  TaskQueueRepo& repo,
                  OllamaClient& ollama,
                  ContentExtractorFactory& factory)
      : store_(store), task_repo_(repo), ollama_client_(ollama), content_extractor_fac_(factory) {}

  // Public getters for each service
  MetadataStore& get_metadata_store() {
    return store_;
  }
  TaskQueueRepo& get_task_queue_repo() {
    return task_repo_;
  }
  OllamaClient& get_ollama_client() {
    return ollama_client_;
  }
  ContentExtractorFactory& get_extractor_factory() {
    return content_extractor_fac_;
  }

 private:
  MetadataStore& store_;
  TaskQueueRepo& task_repo_;
  OllamaClient& ollama_client_;
  ContentExtractorFactory& content_extractor_fac_;
};

}  // namespace magic_core