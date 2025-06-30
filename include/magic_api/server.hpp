#pragma once
#include <crow.h>

#include <string>

namespace magic_api {
class Server {
 public:
  Server(const std::string &host, int port);
  ~Server() = default;

  // Disable move and copy operations since crow::SimpleApp doesn't support them
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server(Server &&) = delete;
  Server &operator=(Server &&) = delete;

  // Get the Crow app reference for route registration
  crow::SimpleApp &get_app() {
    return app_;
  }

  void start();

  void stop();

  bool is_running() const {
    return running_;
  }

  std::string get_host() const {
    return host_;
  }
  int get_port() const {
    return port_;
  }

 private:
  crow::SimpleApp app_;
  std::string host_;
  int port_;
  bool running_ = false;
};
}