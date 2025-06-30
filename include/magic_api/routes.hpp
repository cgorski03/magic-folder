#pragma once

#include "server.hpp"
#include <memory>

namespace magic_core
{
  class OllamaClient;
  class VectorStore;
  class MetadataStore;
  class ContentExtractor;
}

namespace magic_api
{

  class Routes
  {
  public:
    Routes(std::shared_ptr<magic_core::OllamaClient> ollama_client,
           std::shared_ptr<magic_core::VectorStore> vector_store,
           std::shared_ptr<magic_core::MetadataStore> metadata_store,
           std::shared_ptr<magic_core::ContentExtractor> content_extractor);
    ~Routes();

    // Disable copy constructor and assignment
    Routes(const Routes &) = delete;
    Routes &operator=(const Routes &) = delete;

    // Allow move constructor and assignment
    Routes(Routes &&) noexcept;
    Routes &operator=(Routes &&) noexcept;

    // Register all routes with the server
    void register_routes(Server &server);

  private:
    std::shared_ptr<magic_core::OllamaClient> ollama_client_;
    std::shared_ptr<magic_core::VectorStore> vector_store_;
    std::shared_ptr<magic_core::MetadataStore> metadata_store_;
    std::shared_ptr<magic_core::ContentExtractor> content_extractor_;

    // Route handlers
    HttpResponse handle_health_check(const HttpRequest &request);
    HttpResponse handle_process_file(const HttpRequest &request);
    HttpResponse handle_search(const HttpRequest &request);
    HttpResponse handle_list_files(const HttpRequest &request);
    HttpResponse handle_get_file_info(const HttpRequest &request);
    HttpResponse handle_delete_file(const HttpRequest &request);

    // Helper methods
    nlohmann::json parse_json_body(const std::string &body);
    std::string extract_file_path_from_request(const HttpRequest &request);
    std::string extract_search_query_from_request(const HttpRequest &request);
    int extract_top_k_from_request(const HttpRequest &request);
    nlohmann::json create_success_response(const std::string &message, const nlohmann::json &data = nullptr);
    nlohmann::json create_error_response(const std::string &error);
  };

} // namespace magic_api