#pragma once
#include <memory>
#include <nlohmann/json.hpp>

#include "server.hpp"

namespace magic_core {
class OllamaClient;
class MetadataStore;
class ContentExtractor;
}  // namespace magic_core

namespace magic_api {

class Routes {
 public:
  Routes(std::shared_ptr<magic_core::OllamaClient> ollama_client,
         std::shared_ptr<magic_core::MetadataStore> metadata_store,
         std::shared_ptr<magic_core::ContentExtractor> content_extractor);
  ~Routes() = default;

  // Disable copy constructor and assignment
  Routes(const Routes &) = delete;
  Routes &operator=(const Routes &) = delete;

  // Allow move constructor and assignment
  Routes(Routes &&) noexcept = default;
  Routes &operator=(Routes &&) noexcept = default;

  // Register all routes with the server
  void register_routes(Server &server);

 private:
  std::shared_ptr<magic_core::OllamaClient> ollama_client_;
  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::shared_ptr<magic_core::ContentExtractor> content_extractor_;

  // Route handlers
  crow::response handle_health_check(const crow::request &req);
  crow::response handle_process_file(const crow::request &req);
  crow::response handle_search(const crow::request &req);
  crow::response handle_list_files(const crow::request &req);
  crow::response handle_get_file_info(const crow::request &req, const std::string &path);
  crow::response handle_delete_file(const crow::request &req, const std::string &path);

  // Helper methods
  nlohmann::json parse_json_body(const std::string &body);
  std::string extract_file_path_from_request(const crow::request &req);
  std::string extract_search_query_from_request(const crow::request &req);
  int extract_top_k_from_request(const crow::request &req);
  nlohmann::json create_success_response(const std::string &message,
                                         const nlohmann::json &data = nlohmann::json{});
  nlohmann::json create_error_response(const std::string &error);
  crow::response create_json_response(const nlohmann::json &json_data, int status_code = 200);
};

}  // namespace magic_api