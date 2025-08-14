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

  crow::SimpleApp &get_app() {
    return app_;
  }

  void start();

  void stop();

  bool is_running() const {
    return running_;
  }

 private:
  crow::SimpleApp app_;
  std::string host_;
  int port_;
  std::future<void> server_thread_future_;  // Manages the server thread
  bool running_ = false;
};
}  // namespace magic_api