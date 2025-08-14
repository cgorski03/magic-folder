#include "magic_api/server.hpp"

namespace magic_api {
Server::Server(const std::string &host, int port) : host_(host), port_(port), running_(false) {}
void Server::start() {
  if (running_) {
    return;
  }
  running_ = true;
  server_thread_future_ =
      std::async(std::launch::async, [this] { app_.port(port_).bindaddr(host_).run(); });
}

void Server::stop() {
  if (!running_) {
    return;
  }
  app_.stop();

  if (server_thread_future_.valid()) {
    server_thread_future_.get();
  }
  running_ = false;
}
}  // namespace magic_api