#include "magic_api/server.hpp"
#include <iostream>
#include <crow_all.h>

namespace magic_api
{

  // Platform-specific implementation placeholder
  struct Server::Impl
  {
    std::string host;
    int port;
    bool running;
    crow::SimpleApp app;
    // TODO: Add actual HTTP server implementation
    // Could use libraries like:
    // - cpp-httplib
    // - Crow
    // - Beast (Boost.Beast)
    // - Simple HTTP server with sockets
  };

  Server::Server(const std::string &host, int port)
      : host_(host), port_(port), running_(false), pimpl_(std::make_unique<Impl>())
  {
    pimpl_->host = host;
    pimpl_->port = port;
    pimpl_->running = false;
  }

  Server::~Server()
  {
    stop();
  }

  Server::Server(Server &&other) noexcept
      : host_(std::move(other.host_)), port_(other.port_), running_(other.running_), routes_(std::move(other.routes_)), pimpl_(std::move(other.pimpl_))
  {
  }

  Server &Server::operator=(Server &&other) noexcept
  {
    if (this != &other)
    {
      stop();
      host_ = std::move(other.host_);
      port_ = other.port_;
      running_ = other.running_;
      routes_ = std::move(other.routes_);
      pimpl_ = std::move(other.pimpl_);
    }
    return *this;
  }

  void Server::add_route(const std::string &path, std::function<crow::response(const crow::request &)> handler)
  {
    CROW_ROUTE(pimpl_->app, path).methods("GET"_method, "POST"_method)(handler);
    std::cout << "Added route: " << path << std::endl;
  }

  void Server::start()
  {
    if (running_)
    {
      std::cout << "Server is already running" << std::endl;
      return;
    }

    running_ = true;
    pimpl_->running = true;

    std::cout << "Starting HTTP server on " << host_ << ":" << port_ << std::endl;
    std::cout << "Available routes:" << std::endl;
    for (const auto &route : routes_)
    {
      std::cout << "  - " << route.first << std::endl;
    }

    pimpl_->app.port(port_).multithreaded().run();
  }

  void Server::stop()
  {
    if (!running_)
    {
      return;
    }

    running_ = false;
    pimpl_->running = false;

    std::cout << "Stopping HTTP server..." << std::endl;
  }

  bool Server::is_running() const
  {
    return running_;
  }

  std::string Server::get_host() const
  {
    return host_;
  }

  int Server::get_port() const
  {
    return port_;
  }

  std::string Server::method_to_string(HttpMethod method)
  {
    switch (method)
    {
    case HttpMethod::GET:
      return "GET";
    case HttpMethod::POST:
      return "POST";
    case HttpMethod::PUT:
      return "PUT";
    case HttpMethod::DELETE:
      return "DELETE";
    default:
      return "UNKNOWN";
    }
  }

  std::string Server::create_route_key(HttpMethod method, const std::string &path)
  {
    return method_to_string(method) + ":" + path;
  }

  HttpResponse Server::handle_request(const HttpRequest &request)
  {
    std::string key = create_route_key(request.method, request.path);

    auto it = routes_.find(key);
    if (it != routes_.end())
    {
      return it->second(request);
    }

    return create_error_response(404, "Route not found: " + request.path);
  }

  HttpResponse Server::create_error_response(int status_code, const std::string &message)
  {
    HttpResponse response(status_code);
    response.body = message;
    response.headers["Content-Type"] = "text/plain";
    return response;
  }

  HttpResponse Server::create_json_response(const nlohmann::json &json_data)
  {
    HttpResponse response;
    response.body = json_data.dump(2);
    response.headers["Content-Type"] = "application/json";
    return response;
  }

} // namespace magic_api