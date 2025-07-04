#include "magic_cli/cli_handler.hpp"
#include <iostream>
#include <sstream>

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
            }
        }
        if (options.query.empty()) {
            throw CliError("Search command requires a query. Usage: search --query <query>");
        }
    } else if (command == "list" || command == "l") {
        options.command = Command::List;
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
    std::cout << "Searching for: " << options.query << " (top_k: " << options.top_k << ")" << std::endl;
    
    nlohmann::json request_data = {
        {"query", options.query},
        {"top_k", options.top_k}
    };
    
    try {
        nlohmann::json response = make_post_request("/search", request_data);
        print_json_response(response);
    } catch (const std::exception& e) {
        print_error("Failed to search: " + std::string(e.what()));
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

void CliHandler::print_error(const std::string& error) {
    std::cerr << "Error: " << error << std::endl;
}

void CliHandler::print_help() {
    std::cout << R"(
Magic Folder CLI - Intelligent File Management System

Usage: magic_cli <command> [options]

Commands:
  process, p    Process a file for indexing
    --file, -f <path>    Path to the file to process

  search, s     Search for files using semantic search
    --query, -q <query>  Search query
    --top-k, -k <num>    Number of results to return (default: 5)

  list, l       List all indexed files

  help, h       Show this help message

Environment Variables:
  API_BASE_URL  Base URL for the Magic Folder API (default: http://127.0.0.1:3030)

Examples:
  magic_cli process --file /path/to/document.txt
  magic_cli search --query "machine learning algorithms" --top-k 10
  magic_cli list
)" << std::endl;
}

std::string CliHandler::build_url(const std::string& endpoint) {
    return api_base_url_ + endpoint;
}

} // namespace magic_cli 