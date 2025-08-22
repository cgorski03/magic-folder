#include "magic_core/async/file_watcher_service.hpp"

#include <iostream>

#include "magic_core/db/metadata_store.hpp"
#include "magic_core/db/task_queue_repo.hpp"

namespace magic_core::async {

// Forward-declared in the mac backend .mm file
std::unique_ptr<IFileWatcherBackend> make_mac_fsevents_backend(
    const std::filesystem::path& root, IFileWatcherBackend::Handler handler);

// ---------- FileWatcherService public ----------

FileWatcherService::FileWatcherService(const WatchConfig& cfg,
                                       TaskQueueRepo& tasks,
                                       MetadataStore& metadata)
    : cfg_(cfg), tasks_(tasks), metadata_(metadata) {
#if defined(__APPLE__)
  backend_ = make_mac_fsevents_backend(
      cfg_.drop_root,
      [this](const FileWatchEvent& ev) { this->on_backend_event(ev); });
#else
  static_assert(true, "This implementation currently targets macOS only.");
#endif
}

void FileWatcherService::start() {
  if (running_.load()) return;
  running_.store(true);

  // Start backend stream (its own thread/runloop)
  backend_->start();

  // Start settle loop (debounce new files)
  settle_thread_ = std::thread([this] { this->settle_loop(); });

  // Start dirty sweeper (batch reindex for edits)
  sweeper_thread_ = std::thread([this] { this->dirty_sweeper_loop(); });
}

void FileWatcherService::stop() {
  if (!running_.load()) return;
  running_.store(false);

  backend_->stop();

  if (settle_thread_.joinable()) settle_thread_.join();
  if (sweeper_thread_.joinable()) sweeper_thread_.join();
}

void FileWatcherService::initial_scan() {
  // Always increment scan count, even if directory doesn't exist
  {
    std::lock_guard<std::mutex> lk(stats_mu_);
    stats_.scans_performed++;
  }
  
  std::error_code ec;
  if (!std::filesystem::exists(cfg_.drop_root, ec)) {
    std::cerr << "[Watcher] Drop root does not exist: "
              << cfg_.drop_root.string() << std::endl;
    return;
  }

  const auto opts = std::filesystem::directory_options::skip_permission_denied;

  auto scan_file = [&](const std::filesystem::path& p) {
    if (ignore_path(p)) return;
    FileWatchEvent ev;
    ev.path = p;
    ev.is_dir = false;
    ev.kind = EventKind::Created;
    ev.ts = std::chrono::system_clock::now();
    coalesce_created_or_movedin(p);
    std::lock_guard<std::mutex> lk(stats_mu_);
    stats_.events_seen++;
  };

  try {
    if (cfg_.recursive) {
      for (auto it = std::filesystem::recursive_directory_iterator(cfg_.drop_root, opts);
           it != std::filesystem::recursive_directory_iterator(); ++it) {
        std::error_code ec2;
        if (!it->is_regular_file(ec2)) continue;
        scan_file(it->path());
      }
    } else {
      for (auto& entry : std::filesystem::directory_iterator(cfg_.drop_root, opts)) {
        std::error_code ec2;
        if (!entry.is_regular_file(ec2)) continue;
        scan_file(entry.path());
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "[Watcher] initial_scan error: " << e.what() << std::endl;
  }
}

WatcherStats FileWatcherService::stats() const {
  std::lock_guard<std::mutex> lk(stats_mu_);
  return stats_;
}

// ---------- backend event handling ----------

void FileWatcherService::on_backend_event(const FileWatchEvent& ev) {
  {
    std::lock_guard<std::mutex> lk(stats_mu_);
    stats_.events_seen++;
  }

  std::cout << "[FileWatcher] Event received: path=" << ev.path.string() 
            << ", kind=" << static_cast<int>(ev.kind) 
            << ", is_dir=" << ev.is_dir << std::endl;

  // Handle overflow events first - they don't have meaningful paths
  if (ev.kind == EventKind::Overflow) {
    std::cout << "[FileWatcher] Processing overflow event" << std::endl;
    handle_overflow();
    return;
  }

  if (ev.is_dir) {
    std::cout << "[FileWatcher] Ignoring directory event: " << ev.path.string() << std::endl;
    return;
  }
  
  if (ignore_path(ev.path)) {
    std::cout << "[FileWatcher] Ignoring path due to filter: " << ev.path.string() << std::endl;
    return;
  }

  switch (ev.kind) {
    case EventKind::Created:
      std::cout << "[FileWatcher] Processing CREATED event for: " << ev.path.string() << std::endl;
      coalesce_created_or_movedin(ev.path);
      break;
    case EventKind::Modified:
      std::cout << "[FileWatcher] Processing MODIFIED event for: " << ev.path.string() << std::endl;
      handle_modified(ev.path);
      break;
    case EventKind::Renamed:
      std::cout << "[FileWatcher] Processing RENAMED event for: " << ev.path.string() << std::endl;
      if (ev.old_path) {
        handle_renamed(*ev.old_path, ev.path);
      } else {
        // FSEvents doesn't provide old path reliably; treat as Created
        coalesce_created_or_movedin(ev.path);
      }
      break;
    case EventKind::Deleted:
      std::cout << "[FileWatcher] Processing DELETED event for: " << ev.path.string() << std::endl;
      handle_deleted(ev.path);
      break;
    case EventKind::Overflow:
      // This case should never be reached now
      std::cout << "[FileWatcher] Processing OVERFLOW event (fallback)" << std::endl;
      handle_overflow();
      break;
  }
}

// ---------- periodic loops ----------

void FileWatcherService::settle_loop() {
  while (running_.load()) {
    std::vector<std::filesystem::path> ready;

    {
      std::lock_guard<std::mutex> lk(seen_mu_);
      const auto now = Clock::now();

      if (!seen_.empty()) {
        std::cout << "[FileWatcher] Settle loop checking " << seen_.size() << " files" << std::endl;
      }

      for (auto it = seen_.begin(); it != seen_.end();) {
        const auto& path = it->first;
        auto& e = it->second;

        std::error_code ec;
        const auto cur_size = std::filesystem::file_size(path, ec);
        const auto cur_mtime = std::filesystem::last_write_time(path, ec);

        const bool unchanged = (!ec && cur_size == e.last_size &&
                                cur_mtime == e.last_mtime);
        const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - e.last_seen).count();
        const bool aged = (now - e.last_seen) >= cfg_.settle_ms;

        std::cout << "[FileWatcher] File " << path.string() 
                  << " - age: " << age_ms << "ms (need: " << cfg_.settle_ms.count() << "ms)"
                  << ", unchanged: " << unchanged 
                  << ", aged: " << aged << std::endl;

        if (unchanged && aged) {
          std::cout << "[FileWatcher] File is stable, marking ready: " << path.string() << std::endl;
          ready.push_back(path);
          it = seen_.erase(it);
        } else {
          // Only update last_seen if the file has actually changed
          if (!ec && (cur_size != e.last_size || cur_mtime != e.last_mtime)) {
            std::cout << "[FileWatcher] File changed, resetting settle timer: " << path.string() << std::endl;
            e.last_size = cur_size;
            e.last_mtime = cur_mtime;
            e.last_seen = now;
          }
          // If unchanged but not aged enough, don't reset the timer
          ++it;
        }
      }
    }

    if (!ready.empty()) {
      std::cout << "[FileWatcher] Settle loop found " << ready.size() << " files ready for processing" << std::endl;
    }
    
    for (const auto& p : ready) {
      enqueue_process_file(p);
      {
        std::lock_guard<std::mutex> lk(stats_mu_);
        stats_.files_enqueued++;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
}

void FileWatcherService::dirty_sweeper_loop() {
  while (running_.load()) {
    const auto cutoff = Clock::now() - cfg_.modify_quiesce_ms;

    std::vector<std::filesystem::path> to_reindex;
    {
      std::lock_guard<std::mutex> lk(dirty_mu_);
      for (auto it = dirty_.begin(); it != dirty_.end();) {
        if (it->second <= cutoff) {
          to_reindex.push_back(it->first);
          it = dirty_.erase(it);
        } else {
          ++it;
        }
      }
    }

    // Cap batch size
    if (to_reindex.size() > cfg_.reindex_batch_size) {
      to_reindex.resize(cfg_.reindex_batch_size);
    }

    for (const auto& p : to_reindex) {
      enqueue_reindex_file(p);
    }

    std::this_thread::sleep_for(cfg_.sweep_interval);
  }
}

// ---------- helpers ----------

bool FileWatcherService::ignore_path(const std::filesystem::path& p) const {
  // Only consider files inside drop_root
  std::error_code ec;
  auto abs_p = std::filesystem::weakly_canonical(p, ec);
  auto abs_root = std::filesystem::weakly_canonical(cfg_.drop_root, ec);
  if (!abs_p.native().empty() && !abs_root.native().empty()) {
    auto s = abs_p.native();
    auto r = abs_root.native();
    if (s.rfind(r, 0) != 0) return true;  // not under root
  }

  if (std::filesystem::is_symlink(p, ec)) return true;

  const auto name = p.filename().string();
  for (const auto& pat : cfg_.ignore_patterns) {
    if (name == pat) return true;
  }
  for (const auto& suf : cfg_.ignore_suffixes) {
    if (name.size() >= suf.size() &&
        name.compare(name.size() - suf.size(), suf.size(), suf) == 0) {
      return true;
    }
  }
  return false;
}

void FileWatcherService::coalesce_created_or_movedin(
    const std::filesystem::path& p) {
  std::error_code ec;
  if (!std::filesystem::exists(p, ec)) {
    std::cout << "[FileWatcher] File does not exist for coalescing: " << p.string() << std::endl;
    return;
  }
  if (!std::filesystem::is_regular_file(p, ec)) {
    std::cout << "[FileWatcher] Path is not a regular file: " << p.string() << std::endl;
    return;
  }

  std::cout << "[FileWatcher] Coalescing file for settling: " << p.string() << std::endl;

  SeenEntry e{};
  e.last_size = std::filesystem::file_size(p, ec);
  e.last_mtime = std::filesystem::last_write_time(p, ec);
  e.last_seen = Clock::now();
  e.pending = true;

  std::lock_guard<std::mutex> lk(seen_mu_);
  seen_[p] = e;
}

void FileWatcherService::handle_modified(const std::filesystem::path& p) {
  std::lock_guard<std::mutex> lk(dirty_mu_);
  dirty_[p] = Clock::now();
  {
    std::lock_guard<std::mutex> lk2(stats_mu_);
    stats_.files_marked_dirty++;
  }
}

void FileWatcherService::handle_renamed(const std::filesystem::path& from,
                                        const std::filesystem::path& to) {
  try {
    update_path_in_db_if_present(from, to);
  } catch (const std::exception& e) {
    std::cerr << "[Watcher] handle_renamed DB update failed: " << e.what()
              << std::endl;
  }
  // Treat new path as a possible new/updated file
  coalesce_created_or_movedin(to);
}

void FileWatcherService::handle_deleted(const std::filesystem::path& p) {
  {
    std::lock_guard<std::mutex> lk(seen_mu_);
    seen_.erase(p);
  }
  {
    std::lock_guard<std::mutex> lk(dirty_mu_);
    dirty_.erase(p);
  }
  try {
    mark_removed_in_db_if_present(p);
  } catch (const std::exception& e) {
    std::cerr << "[Watcher] handle_deleted DB update failed: " << e.what()
              << std::endl;
  }
}

void FileWatcherService::handle_overflow() {
  {
    std::lock_guard<std::mutex> lk(stats_mu_);
    stats_.overflows++;
  }
  // Best-effort reconcile
  initial_scan();
}

void FileWatcherService::enqueue_process_file(const std::filesystem::path& p) {
  try {
    std::cout << "[FileWatcher] Enqueueing PROCESS_FILE task for: " << p.string() << std::endl;
    tasks_.enqueue_process_file(p.string(), /*priority=*/10);
    std::cout << "[FileWatcher] Successfully enqueued PROCESS_FILE task for: " << p.string() << std::endl;
  } catch (const std::exception& e) {
    // Likely duplicate due to idempotent unique constraint; safe to ignore
    std::cerr << "[FileWatcher] enqueue_process_file error: " << e.what() << std::endl;
  }
}

void FileWatcherService::enqueue_reindex_file(const std::filesystem::path& p) {
  try {
    tasks_.enqueue_reindex_file(p.string(), /*priority=*/8);
  } catch (const std::exception& e) {
    std::cerr << "[Watcher] enqueue_reindex_file: " << e.what() << std::endl;
  }
}

void FileWatcherService::update_path_in_db_if_present(
    const std::filesystem::path& from, const std::filesystem::path& to) {
  metadata_.update_path_if_exists(from.string(), to.string());
}

void FileWatcherService::mark_removed_in_db_if_present(
    const std::filesystem::path& p) {
  metadata_.mark_removed_if_exists(p.string());
}

}  // namespace magic_core::async