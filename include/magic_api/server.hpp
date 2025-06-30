#pragma once
#include <crow.h>
#include <string>
#include <functional>

namespace magic_api
{

  enum class HttpMethod
  {
    GET,
    POST,
    PUT,
    DELETE
  };

  struct HttpRequest
  {
    HttpMethod method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
  };

  struct HttpResponse
  {
    int status_code;
    std::string body;
    std::unordered_map<std::string, std::string> headers;

    HttpResponse() : status_code(200) {}
    HttpResponse(int code) : status_code(code) {}
  };

  using RouteHandler = std::function<HttpResponse(const HttpRequest &)>;

  class Server
  {
  public:
    Server(const std::string &host, int port);

    // Add a route (GET, POST, etc.)
    template <typename Func>
    void add_route(const std::string &method, const std::string &path, Func &&handler);

    // Start the server
    void start();

    // Stop the server (Crow doesn't support this natively, but you can add logic if needed)
    void stop();

    // Check if server is running
    bool is_running() const;

    // Get server info
    std::string get_host() const;
    int get_port() const;

  private:
    crow::SimpleApp app_;
    std::string host_;
    int port_;
    bool running_ = false;

    // Route handlers
    std::unordered_map<std::string, RouteHandler> routes_;

    // Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    // Helper methods
    std::string method_to_string(HttpMethod method);
    std::string create_route_key(HttpMethod method, const std::string &path);
    HttpResponse handle_request(const HttpRequest &request);
    HttpResponse create_error_response(int status_code, const std::string &message);
    HttpResponse create_json_response(const nlohmann::json &json_data);
  };

}