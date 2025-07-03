#pragma once

#include <atomic>
#include <filesystem>
#include <magic_core/file_watcher.hpp>
#include <magic_services/file_service.hpp>
#include <memory>

namespace magic_services {
namespace background {

class FileWatcherService {
 public:
  FileWatcherService(const std::filesystem::path &directory,
                     std::shared_ptr<magic_services::FileService> file_service);

  void start();
  void stop();
  bool is_running() const;

 private:
  void handle_event(const magic_core::FileEvent &event);

  std::shared_ptr<magic_core::FileWatcher> watcher_;
  std::shared_ptr<magic_services::FileService> file_service_;
  std::atomic<bool> running_{false};
};

}  // namespace background
}  // namespace magic_services