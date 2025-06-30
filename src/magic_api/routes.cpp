#include "magic_api/routes.hpp"
#include <iostream>

namespace magic_api
{

  Routes::Routes(std::shared_ptr<magic_core::OllamaClient> ollama_client,
                 std::shared_ptr<magic_core::VectorStore> vector_store,
                 std::shared_ptr<magic_core::MetadataStore> metadata_store,
                 std::shared_ptr<magic_core::ContentExtractor> content_extractor)
      : ollama_client_(ollama_client), vector_store_(vector_store), metadata_store_(metadata_store), content_extractor_(content_extractor)
  {
  }

  Routes::~Routes() = default;

  Routes::Routes(Routes &&other) noexcept
      : ollama_client_(std::move(other.ollama_client_)), vector_store_(std::move(other.vector_store_)), metadata_store_(std::move(other.metadata_store_)), content_extractor_(std::move(other.content_extractor_))
  {
  }

  Routes &Routes::operator=(Routes &&other) noexcept
  {
    if (this != &other)
    {
      ollama_client_ = std::move(other.ollama_client_);
      vector_store_ = std::move(other.vector_store_);
      metadata_store_ = std::move(other.metadata_store_);
      content_extractor_ = std::move(other.content_extractor_);
    }
    return *this;
  }

  void Routes::register_routes(Server &server)
  {
    // Health check endpoint
    server.add_route(HttpMethod::GET, "/",
                     [this](const HttpRequest &req)
                     { return handle_health_check(req); });

    // File processing endpoint
    server.add_route(HttpMethod::POST, "/process_file",
                     [this](const HttpRequest &req)
                     { return handle_process_file(req); });

    // Search endpoint
    server.add_route(HttpMethod::POST, "/search",
                     [this](const HttpRequest &req)
                     { return handle_search(req); });

    // List files endpoint
    server.add_route(HttpMethod::GET, "/files",
                     [this](const HttpRequest &req)
                     { return handle_list_files(req); });

    // Get file info endpoint
    server.add_route(HttpMethod::GET, "/files/{path}",
                     [this](const HttpRequest &req)
                     { return handle_get_file_info(req); });

    // Delete file endpoint
    server.add_route(HttpMethod::DELETE, "/files/{path}",
                     [this](const HttpRequest &req)
                     { return handle_delete_file(req); });
  }

  HttpResponse Routes::handle_health_check(const HttpRequest &request)
  {
    nlohmann::json response = create_success_response("Magic Folder API is running");
    response["version"] = "0.1.0";
    response["status"] = "healthy";

    HttpResponse http_response;
    http_response.status_code = 200;
    http_response.body = response.dump(2);
    http_response.headers["Content-Type"] = "application/json";

    return http_response;
  }

  HttpResponse Routes::handle_process_file(const HttpRequest &request)
  {
    try
    {
      std::string file_path = extract_file_path_from_request(request);

      // TODO: Implement actual file processing logic
      std::cout << "Processing file: " << file_path << std::endl;

      nlohmann::json response = create_success_response("File processed successfully");
      response["file_id"] = 1; // TODO: Get actual file ID
      response["vector_id"] = file_path;

      HttpResponse http_response;
      http_response.status_code = 200;
      http_response.body = response.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
    catch (const std::exception &e)
    {
      nlohmann::json error_response = create_error_response(e.what());

      HttpResponse http_response;
      http_response.status_code = 400;
      http_response.body = error_response.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
  }

  HttpResponse Routes::handle_search(const HttpRequest &request)
  {
    try
    {
      std::string query = extract_search_query_from_request(request);
      int top_k = extract_top_k_from_request(request);

      // TODO: Implement actual search logic
      std::cout << "Searching for: " << query << " with top_k: " << top_k << std::endl;

      // Placeholder search results
      nlohmann::json results = nlohmann::json::array();
      nlohmann::json result;
      result["path"] = "example.txt";
      result["score"] = 0.85;
      results.push_back(result);

      HttpResponse http_response;
      http_response.status_code = 200;
      http_response.body = results.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
    catch (const std::exception &e)
    {
      nlohmann::json error_response = create_error_response(e.what());

      HttpResponse http_response;
      http_response.status_code = 400;
      http_response.body = error_response.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
  }

  HttpResponse Routes::handle_list_files(const HttpRequest &request)
  {
    try
    {
      // TODO: Implement actual file listing logic
      std::cout << "Listing files" << std::endl;

      nlohmann::json results = nlohmann::json::array();
      // TODO: Get actual file list from metadata store

      HttpResponse http_response;
      http_response.status_code = 200;
      http_response.body = results.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
    catch (const std::exception &e)
    {
      nlohmann::json error_response = create_error_response(e.what());

      HttpResponse http_response;
      http_response.status_code = 400;
      http_response.body = error_response.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
  }

  HttpResponse Routes::handle_get_file_info(const HttpRequest &request)
  {
    try
    {
      std::string file_path = request.path.substr(7); // Remove "/files/" prefix

      // TODO: Implement actual file info retrieval
      std::cout << "Getting file info for: " << file_path << std::endl;

      nlohmann::json file_info;
      file_info["path"] = file_path;
      file_info["size"] = 1024;   // TODO: Get actual file size
      file_info["type"] = "text"; // TODO: Get actual file type

      HttpResponse http_response;
      http_response.status_code = 200;
      http_response.body = file_info.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
    catch (const std::exception &e)
    {
      nlohmann::json error_response = create_error_response(e.what());

      HttpResponse http_response;
      http_response.status_code = 400;
      http_response.body = error_response.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
  }

  HttpResponse Routes::handle_delete_file(const HttpRequest &request)
  {
    try
    {
      std::string file_path = request.path.substr(8); // Remove "/files/" prefix

      // TODO: Implement actual file deletion
      std::cout << "Deleting file: " << file_path << std::endl;

      nlohmann::json response = create_success_response("File deleted successfully");

      HttpResponse http_response;
      http_response.status_code = 200;
      http_response.body = response.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
    catch (const std::exception &e)
    {
      nlohmann::json error_response = create_error_response(e.what());

      HttpResponse http_response;
      http_response.status_code = 400;
      http_response.body = error_response.dump(2);
      http_response.headers["Content-Type"] = "application/json";

      return http_response;
    }
  }

  nlohmann::json Routes::parse_json_body(const std::string &body)
  {
    try
    {
      return nlohmann::json::parse(body);
    }
    catch (const nlohmann::json::exception &e)
    {
      throw std::runtime_error("Invalid JSON in request body: " + std::string(e.what()));
    }
  }

  std::string Routes::extract_file_path_from_request(const HttpRequest &request)
  {
    nlohmann::json body = parse_json_body(request.body);

    if (!body.contains("file_path"))
    {
      throw std::runtime_error("Missing 'file_path' in request body");
    }

    return body["file_path"].get<std::string>();
  }

  std::string Routes::extract_search_query_from_request(const HttpRequest &request)
  {
    nlohmann::json body = parse_json_body(request.body);

    if (!body.contains("query"))
    {
      throw std::runtime_error("Missing 'query' in request body");
    }

    return body["query"].get<std::string>();
  }

  int Routes::extract_top_k_from_request(const HttpRequest &request)
  {
    nlohmann::json body = parse_json_body(request.body);

    if (body.contains("top_k"))
    {
      return body["top_k"].get<int>();
    }

    return 5; // Default value
  }

  nlohmann::json Routes::create_success_response(const std::string &message, const nlohmann::json &data)
  {
    nlohmann::json response;
    response["message"] = message;
    if (data != nullptr)
    {
      response["data"] = data;
    }
    return response;
  }

  nlohmann::json Routes::create_error_response(const std::string &error)
  {
    nlohmann::json response;
    response["error"] = error;
    return response;
  }

} // namespace magic_api