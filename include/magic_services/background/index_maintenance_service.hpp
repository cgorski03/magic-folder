#pragma once

#include <atomic>
#include <chrono>
#include <magic_core/metadata_store.hpp>
#include <memory>
#include <thread>

namespace magic_services {
namespace background {

class IndexMaintenanceService {
 public:
  explicit IndexMaintenanceService(std::shared_ptr<magic_core::MetadataStore> metadata_store,
                                   std::chrono::seconds interval = std::chrono::hours(1));

  void start();
  void stop();
  bool is_running() const;

 private:
  void maintenance_loop();

  std::shared_ptr<magic_core::MetadataStore> metadata_store_;
  std::chrono::seconds interval_;
  std::unique_ptr<std::thread> worker_;
  std::atomic<bool> running_{false};
};

}  // namespace background
}  // namespace magic_services