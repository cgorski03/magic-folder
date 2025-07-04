#include "magic_api/server.hpp"

#include <iostream>

namespace magic_api {
Server::Server(const std::string &host, int port) : host_(host), port_(port), running_(false) {}

void Server::start() {
  if (running_) {
    std::cout << "Server is already running" << std::endl;
    return;
  }

  running_ = true;
  std::cout << "Starting HTTP server on " << host_ << ":" << port_ << std::endl;

  app_.port(port_).multithreaded().run();
}

void Server::stop() {
  if (!running_) {
    return;
  }

  running_ = false;
  std::cout << "Stopping HTTP server..." << std::endl;
}
}