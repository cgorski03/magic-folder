#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace magic_core {

class TaskQueueRepo;
class MetadataStore;
class FileProcessingService;

namespace async {

// High-level event kind we care about (abstracted from FSEvents details)
enum class EventKind {
  Created,   // file created or moved into the drop root
  Modified,  // content write/save
  Renamed,   // path changed within the drop root
  Deleted,   // removed or moved out of the drop root
  Overflow   // backend told us to rescan (events dropped)
};

struct FileWatchEvent {
  std::filesystem::path path;
  std::optional<std::filesystem::path> old_path;  // for Renamed
  bool is_dir = false;
  EventKind kind = EventKind::Modified;
  std::chrono::system_clock::time_point ts{};
};

// Watcher behavior/configuration knobs
struct WatchConfig {
  std::filesystem::path drop_root;  // The root users manage (recursively watched)
  bool recursive = true;

  // New files are debounced: must be stable (size/mtime unchanged) for this long
  std::chrono::milliseconds settle_ms{1500};

  // Modified files are marked dirty; only reindexed after this idle time passes
  std::chrono::milliseconds modify_quiesce_ms{std::chrono::minutes(5)};

  // Dirty sweeper wake period and per-iteration cap
  std::chrono::milliseconds sweep_interval{std::chrono::seconds(60)};
  std::size_t reindex_batch_size = 50;

  // Ignore patterns (filenames or simple suffix checks)
  std::vector<std::string> ignore_patterns{
      ".DS_Store", "Thumbs.db", ".Spotlight-V100", ".fseventsd"};

  // Simple suffix-based ignores (e.g., .tmp, .part, .crdownload)
  std::vector<std::string> ignore_suffixes{".tmp", ".part", ".download",
                                           ".crdownload"};
};

// Minimal interface for backends (macOS FSEvents implementation lives in .mm)
class IFileWatcherBackend {
public:
  using Handler = std::function<void(const FileWatchEvent&)>;

  virtual ~IFileWatcherBackend() = default;
  virtual void start() = 0;  // runs in its own thread/runloop
  virtual void stop() = 0;   // stops the stream/runloop
};

// Statistics snapshot (optional but handy for observability)
struct WatcherStats {
  uint64_t events_seen = 0;
  uint64_t files_enqueued = 0;   // PROCESS_FILE tasks
  uint64_t files_marked_dirty = 0;
  uint64_t overflows = 0;
  uint64_t scans_performed = 0;  // initial/overflow rescans
};

// FileWatcherService: producer that turns FS events into queue work
class FileWatcherService {
public:
  using Clock = std::chrono::steady_clock;

  FileWatcherService(const WatchConfig& cfg,
                     FileProcessingService& file_processing_service,
                     TaskQueueRepo& task_queue_repo,
                     MetadataStore& metadata);

  // Non-copyable
  FileWatcherService(const FileWatcherService&) = delete;
  FileWatcherService& operator=(const FileWatcherService&) = delete;

  // Start background threads: backend stream, settle loop, dirty sweeper
  void start();

  // Stop everything and join threads (safe to call multiple times)
  void stop();

  bool is_running() const { return running_.load(); }

  // Optional: run a one-time synchronous scan to reconcile state at startup
  // (new/changed/deleted within drop_root)
  void initial_scan();

  // Lightweight read-only stats
  WatcherStats stats() const;

protected:
  // Backend callback -> ingestion/coalescing (exposed for testing)
  void on_backend_event(const FileWatchEvent& ev);

  // Backend pointer (exposed for testing)
  std::unique_ptr<IFileWatcherBackend> backend_;

private:
  // Internal coalescer entry for “settle until stable” logic
  struct SeenEntry {
    uintmax_t last_size = 0;
    std::filesystem::file_time_type last_mtime{};
    Clock::time_point last_seen{};
    bool pending = true;
  };



  // Periodic: drain stable created/moved-in files and enqueue PROCESS_FILE
  void settle_loop();

  // Periodic: find dirty files idle for modify_quiesce_ms and enqueue REINDEX
  void dirty_sweeper_loop();

  // Helpers
  bool ignore_path(const std::filesystem::path& p) const;
  void coalesce_created_or_movedin(const std::filesystem::path& p);
  void handle_modified(const std::filesystem::path& p);
  void handle_renamed(const std::filesystem::path& from,
                      const std::filesystem::path& to);
  void handle_deleted(const std::filesystem::path& p);
  void handle_overflow();

  // Enqueue work (idempotent upsert patterns should be inside TaskQueueRepo)
  void enqueue_process_file(const std::filesystem::path& p);
  void enqueue_reindex_file(const std::filesystem::path& p);

  // Resolve file-id by path and update path/metadata as needed (via MetadataStore)
  void update_path_in_db_if_present(const std::filesystem::path& from,
                                    const std::filesystem::path& to);
  void mark_removed_in_db_if_present(const std::filesystem::path& p);

private:
  // Config and dependencies
  const WatchConfig cfg_;
  FileProcessingService& file_processing_service_;
  TaskQueueRepo& task_queue_repo_;  // Still needed for reindexing workflow
  MetadataStore& metadata_;



  // State and threads
  std::atomic<bool> running_{false};
  std::thread settle_thread_;
  std::thread sweeper_thread_;

  // Coalescer: path -> last snapshot
  mutable std::mutex seen_mu_;
  std::unordered_map<std::filesystem::path, SeenEntry> seen_;

  // Dirty set: path -> last modified timepoint
  mutable std::mutex dirty_mu_;
  std::unordered_map<std::filesystem::path, Clock::time_point> dirty_;

  // Stats (atomic or protected by mutex for infrequent reads)
  mutable std::mutex stats_mu_;
  WatcherStats stats_;
};

}  // namespace async
}  // namespace magic_core