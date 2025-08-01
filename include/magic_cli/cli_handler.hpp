#pragma once

#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace magic_cli
{

  enum class Command
  {
    Process,
    Search,
    FileSearch,
    List,
    Info,
    Delete,
    Help
  };

  struct CliOptions
  {
    Command command;
    std::string file_path;
    std::string query;
    int top_k;
    std::string api_base_url;
    bool verbose;
    bool help;
    bool magic_search;  // true for magic search (files + chunks), false for file-only search
  };

  class CliError : public std::exception
  {
  public:
    explicit CliError(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };

  class CliHandler
  {
  public:
    explicit CliHandler(const std::string &api_base_url);
    ~CliHandler();

    // Disable copy constructor and assignment
    CliHandler(const CliHandler &) = delete;
    CliHandler &operator=(const CliHandler &) = delete;

    // Allow move constructor and assignment
    CliHandler(CliHandler &&) noexcept;
    CliHandler &operator=(CliHandler &&) noexcept;

    // Parse command line arguments
    CliOptions parse_arguments(int argc, char *argv[]);

    // Execute command
    void execute_command(const CliOptions &options);

    // Set API base URL
    void set_api_base_url(const std::string &url);

    // Get API base URL
    std::string get_api_base_url() const;

  private:
    std::string api_base_url_;
    CURL *curl_handle_;

    // Command handlers
    void handle_process_command(const CliOptions &options);
    void handle_search_command(const CliOptions &options);
    void handle_file_search_command(const CliOptions &options);
    void handle_list_command(const CliOptions &options);
    void handle_info_command(const CliOptions &options);
    void handle_delete_command(const CliOptions &options);
    void handle_help_command(const CliOptions &options);

    // HTTP methods
    nlohmann::json make_get_request(const std::string &endpoint);
    nlohmann::json make_post_request(const std::string &endpoint, const nlohmann::json &data);
    nlohmann::json make_delete_request(const std::string &endpoint);

    // Helper methods
    void setup_curl_handle();
    static size_t write_callback(void *contents, size_t size, size_t nmemb, std::string *userp);
    void print_json_response(const nlohmann::json &response);
    void print_magic_search_response(const nlohmann::json &response);
    void print_file_search_response(const nlohmann::json &response);
    void print_error(const std::string &error);
    void print_help();
    std::string build_url(const std::string &endpoint);
  };

}