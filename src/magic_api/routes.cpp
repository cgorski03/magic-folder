#include "magic_api/routes.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

namespace magic_api {
Routes::Routes(std::shared_ptr<magic_core::OllamaClient> ollama_client,
               std::shared_ptr<magic_core::MetadataStore> metadata_store,
               std::shared_ptr<magic_core::ContentExtractor> content_extractor)
    : ollama_client_(ollama_client),
      metadata_store_(metadata_store),
      content_extractor_(content_extractor) {}

void Routes::register_routes(Server &server) {
  auto &app = server.get_app();

  // Health check endpoint
  CROW_ROUTE(app, "/")
  ([this](const crow::request &req) { return handle_health_check(req); });

  // File processing endpoint
  CROW_ROUTE(app, "/process_file")
      .methods(crow::HTTPMethod::POST)(
          [this](const crow::request &req) { return handle_process_file(req); });

  // Search endpoint
  CROW_ROUTE(app, "/search").methods(crow::HTTPMethod::POST)([this](const crow::request &req) {
    return handle_search(req);
  });

  // List files endpoint
  CROW_ROUTE(app, "/files")
  ([this](const crow::request &req) { return handle_list_files(req); });

  // Get file info endpoint
  CROW_ROUTE(app, "/files/<string>")
  ([this](const crow::request &req, const std::string &path) {
    return handle_get_file_info(req, path);
  });

  // Delete file endpoint
  CROW_ROUTE(app, "/files/<string>")
      .methods(crow::HTTPMethod::DELETE)([this](const crow::request &req, const std::string &path) {
        return handle_delete_file(req, path);
      });

  std::cout << "All routes registered successfully" << std::endl;
}

crow::response Routes::handle_health_check(const crow::request &req) {
  nlohmann::json response = create_success_response("Magic Folder API is running");
  response["version"] = "0.1.0";
  response["status"] = "healthy";
  return create_json_response(response);
}

crow::response Routes::handle_process_file(const crow::request &req) {
  try {
    std::string file_path = extract_file_path_from_request(req);
    std::cout << "Processing file: " << file_path << std::endl;
    // TODO: Implement actual file processing
    nlohmann::json response = create_success_response("File processed successfully");
    response["file_id"] = 1;
    response["vector_id"] = file_path;

    return create_json_response(response);
  } catch (const std::exception &e) {
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

// handlers have a lot to do
crow::response Routes::handle_search(const crow::request &req) {
  try {
    std::string query = extract_search_query_from_request(req);
    int top_k = extract_top_k_from_request(req);

    std::cout << "Searching for: " << query << " with top_k: " << top_k << std::endl;

    // Placeholder search results
    nlohmann::json results = nlohmann::json::array();
    nlohmann::json result;
    result["path"] = "example.txt";
    result["score"] = 0.85;
    results.push_back(result);

    return create_json_response(results);
  } catch (const std::exception &e) {
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

crow::response Routes::handle_list_files(const crow::request &req) {
  try {
    std::cout << "Listing files" << std::endl;
    nlohmann::json results = nlohmann::json::array();
    // TODO: Get actual file list from metadata store
    return create_json_response(results);
  } catch (const std::exception &e) {
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

crow::response Routes::handle_get_file_info(const crow::request &req, const std::string &path) {
  try {
    std::cout << "Getting file info for: " << path << std::endl;

    nlohmann::json file_info;
    file_info["path"] = path;
    file_info["size"] = 1024;
    file_info["type"] = "text";

    return create_json_response(file_info);
  } catch (const std::exception &e) {
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

crow::response Routes::handle_delete_file(const crow::request &req, const std::string &path) {
  try {
    std::cout << "Deleting file: " << path << std::endl;
    // TODO: Implement actual file deletion

    nlohmann::json response = create_success_response("File deleted successfully");
    return create_json_response(response);
  } catch (const std::exception &e) {
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

crow::response Routes::create_json_response(const nlohmann::json &json_data, int status_code) {
  crow::response resp(status_code, json_data.dump(2));
  resp.add_header("Content-Type", "application/json");
  return resp;
}

nlohmann::json Routes::create_success_response(const std::string &message,
                                               const nlohmann::json &data) {
  nlohmann::json response;
  response["success"] = true;
  response["message"] = message;
  if (!data.is_null()) {
    response["data"] = data;
  }
  return response;
}

nlohmann::json Routes::create_error_response(const std::string &error) {
  nlohmann::json response;
  response["success"] = false;
  response["error"] = error;
  return response;
}

nlohmann::json Routes::parse_json_body(const std::string &body) {
  return nlohmann::json::parse(body);
}

std::string Routes::extract_file_path_from_request(const crow::request &req) {
  auto json_body = parse_json_body(req.body);
  return json_body.value("file_path", "");
}

std::string Routes::extract_search_query_from_request(const crow::request &req) {
  auto json_body = parse_json_body(req.body);
  return json_body.value("query", "");
}

int Routes::extract_top_k_from_request(const crow::request &req) {
  auto json_body = parse_json_body(req.body);
  return json_body.value("top_k", 10);
}
}