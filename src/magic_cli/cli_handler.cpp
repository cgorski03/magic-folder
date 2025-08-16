#include "magic_cli/cli_handler.hpp"
#include <iostream>
#include <iomanip> // Required for std::fixed and std::setprecision
#include <unordered_map>

namespace magic_cli {

CliHandler::CliHandler(const std::string& api_base_url)
    : api_base_url_(api_base_url), curl_handle_(nullptr) {
    setup_curl_handle();
}

CliHandler::~CliHandler() {
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
    }
}

CliHandler::CliHandler(CliHandler&& other) noexcept
    : api_base_url_(std::move(other.api_base_url_))
    , curl_handle_(other.curl_handle_) {
    other.curl_handle_ = nullptr;
}

CliHandler& CliHandler::operator=(CliHandler&& other) noexcept {
    if (this != &other) {
        if (curl_handle_) {
            curl_easy_cleanup(curl_handle_);
        }
        api_base_url_ = std::move(other.api_base_url_);
        curl_handle_ = other.curl_handle_;
        other.curl_handle_ = nullptr;
    }
    return *this;
}

void CliHandler::setup_curl_handle() {
    curl_handle_ = curl_easy_init();
    if (!curl_handle_) {
        throw CliError("Failed to initialize CURL");
    }
}

size_t CliHandler::write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

CliOptions CliHandler::parse_arguments(int argc, char* argv[]) {
    CliOptions options;
    options.top_k = 5;
    options.verbose = false;
    options.help = false;
    options.magic_search = true;  // Default to magic search
    options.older_than_days = 7;  // Default for clearing tasks
    
    if (argc < 2) {
        options.command = Command::Help;
        return options;
    }
    
    std::string command = argv[1];
    
    if (command == "process" || command == "p") {
        options.command = Command::Process;
        if (argc < 4) {
            throw CliError("Process command requires a file path. Usage: process --file <path>");
        }
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            std::string flag = argv[i];
            std::string value = argv[i + 1];
            
            if (flag == "--file" || flag == "-f") {
                options.file_path = value;
            }
        }
        if (options.file_path.empty()) {
            throw CliError("Process command requires a file path. Usage: process --file <path>");
        }
    } else if (command == "search" || command == "s") {
        options.command = Command::Search;
        options.magic_search = true;  // Default magic search
        if (argc < 4) {
            throw CliError("Search command requires a query. Usage: search --query <query>");
        }
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            std::string flag = argv[i];
            std::string value = argv[i + 1];
            
            if (flag == "--query" || flag == "-q") {
                options.query = value;
            } else if (flag == "--top-k" || flag == "-k") {
                options.top_k = std::stoi(value);
            } else if (flag == "--files-only" || flag == "-f") {
                options.magic_search = false;
            }
        }
        if (options.query.empty()) {
            throw CliError("Search command requires a query. Usage: search --query <query>");
        }
    } else if (command == "filesearch" || command == "fs") {
        options.command = Command::FileSearch;
        options.magic_search = false;  // File-only search
        if (argc < 4) {
            throw CliError("File search command requires a query. Usage: filesearch --query <query>");
        }
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            std::string flag = argv[i];
            std::string value = argv[i + 1];
            
            if (flag == "--query" || flag == "-q") {
                options.query = value;
            } else if (flag == "--top-k" || flag == "-k") {
                options.top_k = std::stoi(value);
            }
        }
        if (options.query.empty()) {
            throw CliError("File search command requires a query. Usage: filesearch --query <query>");
        }
    } else if (command == "list" || command == "l") {
        options.command = Command::List;
    } else if (command == "tasks" || command == "lt") {
        options.command = Command::ListTasks;
        // Parse optional status filter
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            std::string flag = argv[i];
            std::string value = argv[i + 1];
            
            if (flag == "--status" || flag == "-s") {
                options.status_filter = value;
            }
        }
    } else if (command == "task-status" || command == "ts") {
        options.command = Command::TaskStatus;
        if (argc < 4) {
            throw CliError("Task status command requires a task ID. Usage: task-status --id <task_id>");
        }
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            std::string flag = argv[i];
            std::string value = argv[i + 1];
            
            if (flag == "--id" || flag == "-i") {
                options.task_id = value;
            }
        }
        if (options.task_id.empty()) {
            throw CliError("Task status command requires a task ID. Usage: task-status --id <task_id>");
        }
    } else if (command == "task-progress" || command == "tp") {
        options.command = Command::TaskProgress;
        if (argc < 4) {
            throw CliError("Task progress command requires a task ID. Usage: task-progress --id <task_id>");
        }
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            std::string flag = argv[i];
            std::string value = argv[i + 1];
            
            if (flag == "--id" || flag == "-i") {
                options.task_id = value;
            }
        }
        if (options.task_id.empty()) {
            throw CliError("Task progress command requires a task ID. Usage: task-progress --id <task_id>");
        }
    } else if (command == "clear-tasks" || command == "ct") {
        options.command = Command::ClearTasks;
        // Parse optional days parameter
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            std::string flag = argv[i];
            std::string value = argv[i + 1];
            
            if (flag == "--days" || flag == "-d") {
                options.older_than_days = std::stoi(value);
            }
        }
    } else if (command == "help" || command == "h" || command == "--help" || command == "-h") {
        options.command = Command::Help;
    } else {
        throw CliError("Unknown command: " + command);
    }
    
    return options;
}

void CliHandler::execute_command(const CliOptions& options) {
    switch (options.command) {
        case Command::Process:
            handle_process_command(options);
            break;
        case Command::Search:
            handle_search_command(options);
            break;
        case Command::FileSearch:
            handle_file_search_command(options);
            break;
        case Command::List:
            handle_list_command(options);
            break;
        case Command::Info:
            handle_info_command(options);
            break;
        case Command::Delete:
            handle_delete_command(options);
            break;
        case Command::Help:
            handle_help_command(options);
            break;
        // Task management commands
        case Command::ListTasks:
            handle_list_tasks_command(options);
            break;
        case Command::TaskStatus:
            handle_task_status_command(options);
            break;
        case Command::TaskProgress:
            handle_task_progress_command(options);
            break;
        case Command::ClearTasks:
            handle_clear_tasks_command(options);
            break;
    }
}

void CliHandler::handle_process_command(const CliOptions& options) {
    std::cout << "Processing file: " << options.file_path << std::endl;
    
    nlohmann::json request_data = {
        {"file_path", options.file_path}
    };
    
    try {
        nlohmann::json response = make_post_request("/process_file", request_data);
        print_json_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to process file: " + std::string(e.what()));
    }
}

void CliHandler::handle_search_command(const CliOptions& options) {
    std::cout << "Magic search for: " << options.query << " (top_k: " << options.top_k << ")" << std::endl;
    
    nlohmann::json request_data = {
        {"query", options.query},
        {"top_k", options.top_k}
    };
    
    try {
        nlohmann::json response = make_post_request("/search", request_data);
        print_magic_search_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to search: " + std::string(e.what()));
    }
}

void CliHandler::handle_file_search_command(const CliOptions& options) {
    std::cout << "File search for: " << options.query << " (top_k: " << options.top_k << ")" << std::endl;
    
    nlohmann::json request_data = {
        {"query", options.query},
        {"top_k", options.top_k}
    };
    
    try {
        nlohmann::json response = make_post_request("/files/search", request_data);
        print_file_search_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to search files: " + std::string(e.what()));
    }
}

void CliHandler::handle_list_command(const CliOptions& options) {
    std::cout << "Listing files..." << std::endl;
    
    try {
        nlohmann::json response = make_get_request("/files");
        print_json_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to list files: " + std::string(e.what()));
    }
}

void CliHandler::handle_info_command(const CliOptions& options) {
    // TODO: Implement file info command
    std::cout << "File info command not yet implemented" << std::endl;
}

void CliHandler::handle_delete_command(const CliOptions& options) {
    // TODO: Implement file deletion command
    std::cout << "File deletion command not yet implemented" << std::endl;
}

void CliHandler::handle_help_command(const CliOptions& options) {
    print_help();
}

nlohmann::json CliHandler::make_get_request(const std::string& endpoint) {
    if (!curl_handle_) {
        throw CliError("CURL handle not initialized");
    }
    
    std::string url = build_url(endpoint);
    std::string response_buffer;
    
    curl_easy_reset(curl_handle_);
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_buffer);
    
    CURLcode res = curl_easy_perform(curl_handle_);
    if (res != CURLE_OK) {
        throw CliError("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        throw CliError("HTTP request failed with status code: " + std::to_string(http_code));
    }
    
    return nlohmann::json::parse(response_buffer);
}

nlohmann::json CliHandler::make_post_request(const std::string& endpoint, const nlohmann::json& data) {
    if (!curl_handle_) {
        throw CliError("CURL handle not initialized");
    }
    
    std::string url = build_url(endpoint);
    std::string request_json = data.dump();
    std::string response_buffer;
    
    curl_easy_reset(curl_handle_);
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, request_json.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, 
        curl_slist_append(nullptr, "Content-Type: application/json"));
    
    CURLcode res = curl_easy_perform(curl_handle_);
    if (res != CURLE_OK) {
        throw CliError("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        throw CliError("HTTP request failed with status code: " + std::to_string(http_code));
    }
    
    return nlohmann::json::parse(response_buffer);
}

nlohmann::json CliHandler::make_delete_request(const std::string& endpoint) {
    if (!curl_handle_) {
        throw CliError("CURL handle not initialized");
    }
    
    std::string url = build_url(endpoint);
    std::string response_buffer;
    
    curl_easy_reset(curl_handle_);
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_buffer);
    
    CURLcode res = curl_easy_perform(curl_handle_);
    if (res != CURLE_OK) {
        throw CliError("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        throw CliError("HTTP request failed with status code: " + std::to_string(http_code));
    }
    
    return nlohmann::json::parse(response_buffer);
}

void CliHandler::set_api_base_url(const std::string& url) {
    api_base_url_ = url;
}

std::string CliHandler::get_api_base_url() const {
    return api_base_url_;
}

void CliHandler::print_json_response(const nlohmann::json& response) {
    std::cout << response.dump(2) << std::endl;
}

void CliHandler::print_magic_search_response(const nlohmann::json& response) {
    std::cout << "\n=== Magic Search Results ===" << std::endl;
    
    // Create a map from file_id to file_path for chunk display
    std::unordered_map<int, std::string> file_id_to_path;
    
    // Print file results and build the mapping
    if (response.contains("files") && response["files"].is_array()) {
        std::cout << "\n📁 Files:" << std::endl;
        for (const auto& file : response["files"]) {
            std::string path = file["path"].get<std::string>();
            int file_id = file["id"].get<int>();
            file_id_to_path[file_id] = path;
            
            std::cout << "  • " << path 
                      << " (score: " << std::fixed << std::setprecision(3) << file["score"].get<float>() << ")" << std::endl;
        }
    }
    
    // Print chunk results with file information
    if (response.contains("chunks") && response["chunks"].is_array()) {
        std::cout << "\n📄 Chunks:" << std::endl;
        for (const auto& chunk : response["chunks"]) {
            int file_id = chunk["file_id"].get<int>();
            int chunk_index = chunk["chunk_index"].get<int>();
            
            // Get file path from our mapping
            std::string file_path = "Unknown";
            auto it = file_id_to_path.find(file_id);
            if (it != file_id_to_path.end()) {
                file_path = it->second;
            }
            
            std::cout << "  • " << file_id << ":" << chunk_index << " (" << file_path << ")"
                      << " | Score: " << std::fixed << std::setprecision(3) << chunk["score"].get<float>() << std::endl;
            std::cout << "    Content: " << chunk["content"].get<std::string>().substr(0, 100);
            if (chunk["content"].get<std::string>().length() > 100) {
                std::cout << "...";
            }
            std::cout << std::endl << std::endl;
        }
        
        // Print full content of the best match chunk at the bottom
        if (!response["chunks"].empty()) {
            const auto& best_chunk = response["chunks"][0]; // First chunk has the best score
            int file_id = best_chunk["file_id"].get<int>();
            int chunk_index = best_chunk["chunk_index"].get<int>();
            std::string content = best_chunk["content"].get<std::string>();
            float score = best_chunk["score"].get<float>();
            
            // Get file path for the best chunk
            std::string file_path = "Unknown";
            auto it = file_id_to_path.find(file_id);
            if (it != file_id_to_path.end()) {
                file_path = it->second;
            }
            
            std::cout << "\n" << std::string(80, '=') << std::endl;
            std::cout << "🏆 BEST MATCH CHUNK" << std::endl;
            std::cout << std::string(80, '=') << std::endl;
            std::cout << "Location: " << file_id << ":" << chunk_index << " (" << file_path << ")" << std::endl;
            std::cout << "Score: " << std::fixed << std::setprecision(3) << score << std::endl;
            std::cout << std::string(80, '-') << std::endl;
            std::cout << "FULL CONTENT:" << std::endl;
            std::cout << std::string(80, '-') << std::endl;
            std::cout << content << std::endl;
            std::cout << std::string(80, '=') << std::endl;
        }
    }
    
    if (!response.contains("files") && !response.contains("chunks")) {
        std::cout << "No results found." << std::endl;
    }
}

void CliHandler::print_file_search_response(const nlohmann::json& response) {
    std::cout << "\n=== File Search Results ===" << std::endl;
    
    if (response.is_array() && !response.empty()) {
        for (const auto& file : response) {
            std::cout << "  • " << file["path"].get<std::string>() 
                      << " (score: " << std::fixed << std::setprecision(3) << file["score"].get<float>() << ")" << std::endl;
        }
    } else {
        std::cout << "No files found." << std::endl;
    }
}

void CliHandler::print_error(const std::string& error) {
    std::cerr << "Error: " << error << std::endl;
}

void CliHandler::print_help() {
    std::cout << R"(
Magic Folder CLI - Intelligent File Management System

Usage: magic_cli <command> [options]

File Management Commands:
  process, p    Process a file for indexing
    --file, -f <path>    Path to the file to process

  search, s     Magic search for files and chunks using semantic search
    --query, -q <query>  Search query
    --top-k, -k <num>    Number of results to return (default: 5)
    --files-only, -f     Search files only (no chunks)

  filesearch, fs  Search for files only using semantic search
    --query, -q <query>  Search query
    --top-k, -k <num>    Number of results to return (default: 5)

  list, l       List all indexed files

Task Management Commands:
  tasks, lt     List all tasks in the queue
    --status, -s <status>  Filter by task status (PENDING, PROCESSING, COMPLETED, FAILED)

  task-status, ts  Get detailed status of a specific task
    --id, -i <task_id>     Task ID to check

  task-progress, tp  Get progress information for a specific task
    --id, -i <task_id>     Task ID to check

  clear-tasks, ct   Clear completed and failed tasks
    --days, -d <num>       Clear tasks older than N days (default: 7)

General:
  help, h       Show this help message

Environment Variables:
  API_BASE_URL  Base URL for the Magic Folder API (default: http://127.0.0.1:3030)

Examples:
  # File operations
  magic_cli process --file /path/to/document.txt
  magic_cli search --query "machine learning algorithms" --top-k 10
  magic_cli search --query "python code" --files-only
  magic_cli filesearch --query "documentation" --top-k 5
  magic_cli list

  # Task management
  magic_cli tasks                           # List all tasks
  magic_cli tasks --status PENDING         # List only pending tasks
  magic_cli task-status --id 123           # Get status of task 123
  magic_cli task-progress --id 123         # Get progress of task 123
  magic_cli clear-tasks --days 30          # Clear tasks older than 30 days
)" << std::endl;
}

std::string CliHandler::build_url(const std::string& endpoint) {
    return api_base_url_ + endpoint;
}

// ============================================================================
// Task Management Command Handlers
// ============================================================================

void CliHandler::handle_list_tasks_command(const CliOptions& options) {
    std::cout << "Listing tasks";
    if (!options.status_filter.empty()) {
        std::cout << " (status: " << options.status_filter << ")";
    }
    std::cout << std::endl;
    
    try {
        std::string endpoint = "/tasks";
        if (!options.status_filter.empty()) {
            endpoint += "?status=" + options.status_filter;
        }
        
        nlohmann::json response = make_get_request(endpoint);
        print_task_list_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to list tasks: " + std::string(e.what()));
    }
}

void CliHandler::handle_task_status_command(const CliOptions& options) {
    std::cout << "Getting status for task ID: " << options.task_id << std::endl;
    
    try {
        std::string endpoint = "/tasks/" + options.task_id + "/status";
        nlohmann::json response = make_get_request(endpoint);
        print_task_status_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to get task status: " + std::string(e.what()));
    }
}

void CliHandler::handle_task_progress_command(const CliOptions& options) {
    std::cout << "Getting progress for task ID: " << options.task_id << std::endl;
    
    try {
        std::string endpoint = "/tasks/" + options.task_id + "/progress";
        nlohmann::json response = make_get_request(endpoint);
        print_task_progress_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to get task progress: " + std::string(e.what()));
    }
}

void CliHandler::handle_clear_tasks_command(const CliOptions& options) {
    std::cout << "Clearing completed tasks older than " << options.older_than_days << " days" << std::endl;
    
    nlohmann::json request_data = {
        {"older_than_days", options.older_than_days}
    };
    
    try {
        nlohmann::json response = make_post_request("/tasks/clear", request_data);
        print_json_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to clear tasks: " + std::string(e.what()));
    }
}

// ============================================================================
// Task Management Response Printers
// ============================================================================

void CliHandler::print_task_list_response(const nlohmann::json& response) {
    std::cout << "\n=== Task Queue ===" << std::endl;
    
    if (response.contains("success") && response["success"].get<bool>() == true &&
        response.contains("data") && response["data"].contains("tasks")) {
        
        auto tasks = response["data"]["tasks"];
        int count = response["data"]["count"].get<int>();
        
        if (count == 0) {
            std::cout << "No tasks found." << std::endl;
            return;
        }
        
        std::cout << "Found " << count << " task(s):\n" << std::endl;
        
        // Group tasks by status for better display
        std::map<std::string, std::vector<nlohmann::json>> tasks_by_status;
        for (const auto& task : tasks) {
            std::string status = task["status"].get<std::string>();
            tasks_by_status[status].push_back(task);
        }
        
        // Display tasks grouped by status
        for (const auto& [status, status_tasks] : tasks_by_status) {
            std::cout << "=== " << status << " (" << status_tasks.size() << ") ===" << std::endl;
            
            for (const auto& task : status_tasks) {
                std::cout << "  ID: " << task["id"].get<long long>()
                          << " | Type: " << task["task_type"].get<std::string>()
                          << " | Priority: " << task["priority"].get<int>();
                
                if (task.contains("target_path") && !task["target_path"].is_null()) {
                    std::string path = task["target_path"].get<std::string>();
                    if (path.length() > 50) {
                        path = "..." + path.substr(path.length() - 47);
                    }
                    std::cout << " | File: " << path;
                }
                
                if (task.contains("error_message") && !task["error_message"].is_null() && 
                    !task["error_message"].get<std::string>().empty()) {
                    std::string error = task["error_message"].get<std::string>();
                    if (error.length() > 50) {
                        error = error.substr(0, 47) + "...";
                    }
                    std::cout << " | Error: " << error;
                }
                
                std::cout << std::endl;
                std::cout << "    Created: " << task["created_at"].get<std::string>()
                          << " | Updated: " << task["updated_at"].get<std::string>() << std::endl;
                std::cout << std::endl;
            }
        }
    } else if (response.contains("error")) {
        std::cout << "Error: " << response["error"].get<std::string>() << std::endl;
    } else {
        std::cout << "Unexpected response format." << std::endl;
    }
}

void CliHandler::print_task_status_response(const nlohmann::json& response) {
    std::cout << "\n=== Task Status ===" << std::endl;
    
    if (response.contains("success") && response["success"].get<bool>() == true &&
        response.contains("data")) {
        
        auto task = response["data"];
        
        std::cout << "Task ID: " << task["id"].get<long long>() << std::endl;
        std::cout << "Type: " << task["task_type"].get<std::string>() << std::endl;
        std::cout << "Status: " << task["status"].get<std::string>() << std::endl;
        std::cout << "Priority: " << task["priority"].get<int>() << std::endl;
        
        if (task.contains("target_path") && !task["target_path"].is_null()) {
            std::cout << "Target Path: " << task["target_path"].get<std::string>() << std::endl;
        }
        
        if (task.contains("target_tag") && !task["target_tag"].is_null()) {
            std::cout << "Target Tag: " << task["target_tag"].get<std::string>() << std::endl;
        }
        
        if (task.contains("error_message") && !task["error_message"].is_null() && 
            !task["error_message"].get<std::string>().empty()) {
            std::cout << "Error: " << task["error_message"].get<std::string>() << std::endl;
        }
        
        std::cout << "Created: " << task["created_at"].get<std::string>() << std::endl;
        std::cout << "Updated: " << task["updated_at"].get<std::string>() << std::endl;
        
        if (task.contains("payload") && !task["payload"].is_null() && 
            !task["payload"].get<std::string>().empty()) {
            std::cout << "Payload: " << task["payload"].get<std::string>() << std::endl;
        }
        
    } else if (response.contains("error")) {
        std::cout << "Error: " << response["error"].get<std::string>() << std::endl;
    } else {
        std::cout << "Unexpected response format." << std::endl;
    }
}

void CliHandler::print_task_progress_response(const nlohmann::json& response) {
    std::cout << "\n=== Task Progress ===" << std::endl;
    
    if (response.contains("success") && response["success"].get<bool>() == true &&
        response.contains("data")) {
        
        auto progress = response["data"];
        
        std::cout << "Task ID: " << progress["task_id"].get<long long>() << std::endl;
        
        float decimal_progress = progress["progress_percent"].get<float>();
        float percent = decimal_progress * 100.0f;  // Convert decimal to percentage
        std::cout << "Progress: " << std::fixed << std::setprecision(1) << percent << "%" << std::endl;
        
        // Create a simple progress bar
        int bar_width = 40;
        int filled = static_cast<int>(decimal_progress * bar_width);  // Use decimal for bar calculation
        std::cout << "Progress: [";
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) {
                std::cout << "=";
            } else if (i == filled && percent > 0) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << percent << "%" << std::endl;
        
        std::cout << "Status: " << progress["status_message"].get<std::string>() << std::endl;
        std::cout << "Updated: " << progress["updated_at"].get<std::string>() << std::endl;
        
    } else if (response.contains("error")) {
        std::cout << "Error: " << response["error"].get<std::string>() << std::endl;
    } else {
        std::cout << "Unexpected response format." << std::endl;
    }
}

} // namespace magic_cli 