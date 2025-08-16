#include "magic_api/routes.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include "magic_core/services/file_delete_service.hpp"
#include "magic_core/services/file_info_service.hpp"
#include "magic_core/services/file_processing_service.hpp"
#include "magic_core/services/search_service.hpp"
#include "magic_core/db/task_queue_repo.hpp"

namespace magic_api {
Routes::Routes(std::shared_ptr<magic_core::FileProcessingService> file_processing_service,
               std::shared_ptr<magic_core::FileDeleteService> file_delete_service,
               std::shared_ptr<magic_core::FileInfoService> file_info_service,
               std::shared_ptr<magic_core::SearchService> search_service,
               std::shared_ptr<magic_core::TaskQueueRepo> task_queue_repo)
    : file_processing_service_(file_processing_service),
      file_delete_service_(file_delete_service),
      file_info_service_(file_info_service),
      search_service_(search_service),
      task_queue_repo_(task_queue_repo) {}

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

  // File search endpoint
  CROW_ROUTE(app, "/files/search").methods(crow::HTTPMethod::POST)([this](const crow::request &req) {
    return handle_file_search(req);
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

  // Task management endpoints
  CROW_ROUTE(app, "/tasks")
  ([this](const crow::request &req) { return handle_list_tasks(req); });

  CROW_ROUTE(app, "/tasks/<string>/status")
  ([this](const crow::request &req, const std::string &task_id) {
    return handle_get_task_status(req, task_id);
  });

  CROW_ROUTE(app, "/tasks/<string>/progress")
  ([this](const crow::request &req, const std::string &task_id) {
    return handle_get_task_progress(req, task_id);
  });

  CROW_ROUTE(app, "/tasks/clear")
      .methods(crow::HTTPMethod::POST)([this](const crow::request &req) {
        return handle_clear_completed_tasks(req);
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
    // Request processing to add task to queue
    std::optional<long long> task_id = file_processing_service_->request_processing(file_path);
    if (!task_id.has_value()) {
      std::cout << "File already being processed: " << file_path << std::endl;
      return create_json_response(create_error_response("File already being processed"), 400);
    }
    nlohmann::json response = create_success_response("File processing queued successfully");
    return create_json_response(response);
  } catch (const std::exception &e) {
    std::cerr << "Exception in handle_process_file: " << e.what() << std::endl;
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

// handlers have a lot to do
crow::response Routes::handle_search(const crow::request &req) {
  try {
    std::string query = extract_search_query_from_request(req);
    int top_k = extract_top_k_from_request(req);

    std::cout << "Magic search for: " << query << " with top_k: " << top_k << std::endl;

    // Use the magic search that returns both files and chunks
    magic_core::SearchService::MagicSearchResult search_results = search_service_->search(query, top_k);
    
    nlohmann::json response;
    
    // Add file results
    nlohmann::json file_results = nlohmann::json::array();
    for (const magic_core::FileSearchResult &result : search_results.file_results) {
      nlohmann::json result_json;
      result_json["id"] = result.file.id;
      result_json["path"] = result.file.path;
      result_json["score"] = result.distance;
      file_results.push_back(result_json);
    }
    response["files"] = file_results;
    std::cout << "File results: " << file_results.size() << std::endl;
    // Add chunk results
    nlohmann::json chunk_results = nlohmann::json::array();
    for (const magic_core::SearchService::ChunkResultDTO &result : search_results.chunk_results) {
      nlohmann::json result_json;
      result_json["id"] = result.id;
      result_json["file_id"] = result.file_id;
      result_json["chunk_index"] = result.chunk_index;
      result_json["content"] = result.content;
      result_json["score"] = result.distance;
      chunk_results.push_back(result_json);
    }
    response["chunks"] = chunk_results;
    std::cout << "Chunk results: " << chunk_results.size() << std::endl;
    return create_json_response(response);
  } catch (const std::exception &e) {
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

crow::response Routes::handle_file_search(const crow::request &req) {
  try {
    std::string query = extract_search_query_from_request(req);
    int top_k = extract_top_k_from_request(req);

    std::cout << "File search for: " << query << " with top_k: " << top_k << std::endl;

    // Use the file-only search
    std::vector<magic_core::FileSearchResult> search_results = search_service_->search_files(query, top_k);
    
    nlohmann::json results = nlohmann::json::array();
    for (const magic_core::FileSearchResult &result : search_results) {
      nlohmann::json result_json;
      result_json["path"] = result.file.path;
      result_json["score"] = result.distance;
      results.push_back(result_json);
    }

    return create_json_response(results);
  } catch (const std::exception &e) {
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 400);
  }
}

crow::response Routes::handle_list_files(const crow::request &req) {
  try {
    std::cout << "Listing files" << std::endl;
    auto files = file_info_service_->list_files();
    nlohmann::json results = nlohmann::json::array();
    for (const auto &file : files) {
      nlohmann::json file_info;
      file_info["path"] = file.path;
      file_info["size"] = file.file_size;
      file_info["type"] = file.file_type;
      results.push_back(file_info);
    }
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

// ============================================================================
// Task Management Route Handlers
// ============================================================================

crow::response Routes::handle_list_tasks(const crow::request &req) {
  try {
    std::cout << "Listing tasks" << std::endl;
    
    // Parse optional status filter from query parameters
    std::optional<magic_core::TaskStatus> status_filter;
    std::string query_string = req.url_params.get("status") ? req.url_params.get("status") : "";
    
    nlohmann::json tasks_json = nlohmann::json::array();
    
    if (!query_string.empty()) {
      // Filter by specific status
      try {
        magic_core::TaskStatus status = magic_core::task_status_from_string(query_string);
        auto tasks = task_queue_repo_->get_tasks_by_status(status);
        
        for (const auto &task : tasks) {
          nlohmann::json task_json;
          task_json["id"] = task.id;
          task_json["task_type"] = task.task_type;
          task_json["status"] = magic_core::to_string(task.status);
          task_json["priority"] = task.priority;
          task_json["target_path"] = task.target_path;
          task_json["target_tag"] = task.target_tag;
          task_json["payload"] = task.payload;
          task_json["error_message"] = task.error_message;
          task_json["created_at"] = magic_core::TaskQueueRepo::time_point_to_string(task.created_at);
          task_json["updated_at"] = magic_core::TaskQueueRepo::time_point_to_string(task.updated_at);
          tasks_json.push_back(task_json);
        }
      } catch (const std::exception &e) {
        nlohmann::json error_response = create_error_response("Invalid status filter: " + query_string);
        return create_json_response(error_response, 400);
      }
    } else {
      // Get all tasks by iterating through all statuses
      std::vector<magic_core::TaskStatus> all_statuses = {
        magic_core::TaskStatus::PENDING,
        magic_core::TaskStatus::PROCESSING, 
        magic_core::TaskStatus::COMPLETED,
        magic_core::TaskStatus::FAILED
      };
      
      for (const auto &status : all_statuses) {
        auto tasks = task_queue_repo_->get_tasks_by_status(status);
        for (const auto &task : tasks) {
          nlohmann::json task_json;
          task_json["id"] = task.id;
          task_json["task_type"] = task.task_type;
          task_json["status"] = magic_core::to_string(task.status);
          task_json["priority"] = task.priority;
          task_json["target_path"] = task.target_path;
          task_json["target_tag"] = task.target_tag;
          task_json["payload"] = task.payload;
          task_json["error_message"] = task.error_message;
          task_json["created_at"] = magic_core::TaskQueueRepo::time_point_to_string(task.created_at);
          task_json["updated_at"] = magic_core::TaskQueueRepo::time_point_to_string(task.updated_at);
          tasks_json.push_back(task_json);
        }
      }
    }
    
    nlohmann::json response = create_success_response("Tasks retrieved successfully");
    response["data"]["tasks"] = tasks_json;
    response["data"]["count"] = tasks_json.size();
    
    return create_json_response(response);
  } catch (const std::exception &e) {
    std::cerr << "Exception in handle_list_tasks: " << e.what() << std::endl;
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 500);
  }
}

crow::response Routes::handle_get_task_status(const crow::request &req, const std::string &task_id) {
  try {
    std::cout << "Getting task status for task ID: " << task_id << std::endl;
    
    long long id = std::stoll(task_id);
    
    // Get all tasks and find the matching one
    std::vector<magic_core::TaskStatus> all_statuses = {
      magic_core::TaskStatus::PENDING,
      magic_core::TaskStatus::PROCESSING, 
      magic_core::TaskStatus::COMPLETED,
      magic_core::TaskStatus::FAILED
    };
    
    for (const auto &status : all_statuses) {
      auto tasks = task_queue_repo_->get_tasks_by_status(status);
      for (const auto &task : tasks) {
        if (task.id == id) {
          nlohmann::json task_json;
          task_json["id"] = task.id;
          task_json["task_type"] = task.task_type;
          task_json["status"] = magic_core::to_string(task.status);
          task_json["priority"] = task.priority;
          task_json["target_path"] = task.target_path;
          task_json["target_tag"] = task.target_tag;
          task_json["payload"] = task.payload;
          task_json["error_message"] = task.error_message;
          task_json["created_at"] = magic_core::TaskQueueRepo::time_point_to_string(task.created_at);
          task_json["updated_at"] = magic_core::TaskQueueRepo::time_point_to_string(task.updated_at);
          
          nlohmann::json response = create_success_response("Task status retrieved successfully");
          response["data"] = task_json;
          return create_json_response(response);
        }
      }
    }
    
    nlohmann::json error_response = create_error_response("Task not found");
    return create_json_response(error_response, 404);
    
  } catch (const std::invalid_argument &e) {
    nlohmann::json error_response = create_error_response("Invalid task ID format");
    return create_json_response(error_response, 400);
  } catch (const std::exception &e) {
    std::cerr << "Exception in handle_get_task_status: " << e.what() << std::endl;
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 500);
  }
}

crow::response Routes::handle_get_task_progress(const crow::request &req, const std::string &task_id) {
  try {
    std::cout << "Getting task progress for task ID: " << task_id << std::endl;
    
    long long id = std::stoll(task_id);
    auto progress = task_queue_repo_->get_task_progress(id);
    
    if (!progress.has_value()) {
      nlohmann::json error_response = create_error_response("Task progress not found");
      return create_json_response(error_response, 404);
    }
    
    nlohmann::json progress_json;
    progress_json["task_id"] = progress->task_id;
    progress_json["progress_percent"] = progress->progress_percent;
    progress_json["status_message"] = progress->status_message;
    progress_json["updated_at"] = progress->updated_at;
    
    nlohmann::json response = create_success_response("Task progress retrieved successfully");
    response["data"] = progress_json;
    return create_json_response(response);
    
  } catch (const std::invalid_argument &e) {
    nlohmann::json error_response = create_error_response("Invalid task ID format");
    return create_json_response(error_response, 400);
  } catch (const std::exception &e) {
    std::cerr << "Exception in handle_get_task_progress: " << e.what() << std::endl;
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 500);
  }
}

crow::response Routes::handle_clear_completed_tasks(const crow::request &req) {
  try {
    std::cout << "Clearing completed tasks" << std::endl;
    
    // Parse optional days parameter from request body
    int older_than_days = 7; // Default
    if (!req.body.empty()) {
      try {
        auto json_body = parse_json_body(req.body);
        older_than_days = json_body.value("older_than_days", 7);
      } catch (const std::exception &e) {
        // If JSON parsing fails, use default value
        std::cout << "Using default older_than_days value due to parsing error: " << e.what() << std::endl;
      }
    }
    
    task_queue_repo_->clear_completed_tasks(older_than_days);
    
    nlohmann::json response = create_success_response("Completed tasks cleared successfully");
    response["data"]["older_than_days"] = older_than_days;
    return create_json_response(response);
    
  } catch (const std::exception &e) {
    std::cerr << "Exception in handle_clear_completed_tasks: " << e.what() << std::endl;
    nlohmann::json error_response = create_error_response(e.what());
    return create_json_response(error_response, 500);
  }
}

}  // namespace magic_api