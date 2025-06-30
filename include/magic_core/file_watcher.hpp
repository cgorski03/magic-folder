#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <filesystem>
#include <thread>
#include <atomic>

namespace magic_core
{

  enum class FileEventType
  {
    Created,
    Modified,
    Deleted,
    Renamed
  };

  struct FileEvent
  {
    FileEventType type;
    std::filesystem::path path;
    std::filesystem::path old_path; // Only used for Renamed events
  };

  using FileEventHandler = std::function<void(const FileEvent &)>;

  class FileWatcherError : public std::exception
  {
  public:
    explicit FileWatcherError(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };

  class FileWatcher
  {
  public:
    explicit FileWatcher(const std::filesystem::path &directory);
    ~FileWatcher();

    // Disable copy constructor and assignment
    FileWatcher(const FileWatcher &) = delete;
    FileWatcher &operator=(const FileWatcher &) = delete;

    // Allow move constructor and assignment
    FileWatcher(FileWatcher &&) noexcept;
    FileWatcher &operator=(FileWatcher &&) noexcept;

    // Set event handler
    void set_event_handler(FileEventHandler handler);

    // Start watching
    void start();

    // Stop watching
    void stop();

    // Check if watching
    bool is_watching() const;

    // Add directory to watch
    void add_directory(const std::filesystem::path &directory);

    // Remove directory from watch
    void remove_directory(const std::filesystem::path &directory);

    // Get watched directories
    std::vector<std::filesystem::path> get_watched_directories() const;

  private:
    std::vector<std::filesystem::path> directories_;
    FileEventHandler event_handler_;
    std::unique_ptr<std::thread> watch_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;

    // Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    // Helper methods
    void watch_loop();
    void handle_file_event(const FileEvent &event);
  };

}