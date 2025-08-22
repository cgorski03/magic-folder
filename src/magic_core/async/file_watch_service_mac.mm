// Objective-C++ file for macOS FSEvents backend
// Build requires: enable_language(OBJCXX) and link CoreServices/CoreFoundation

#include "magic_core/async/file_watcher_service.hpp"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreServices/CoreServices.h>

#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>

namespace magic_core::async {

class MacFSEventsBackend final : public IFileWatcherBackend {
public:
  MacFSEventsBackend(const std::filesystem::path& root, Handler handler)
      : root_(root), handler_(std::move(handler)) {}

  ~MacFSEventsBackend() override { stop(); }

  void start() override {
    if (running_.load()) return;
    running_.store(true);
    thread_ = std::thread([this] { this->run(); });
  }

  void stop() override {
    if (!running_.load()) return;
    running_.store(false);

    // Stop stream/runloop
    if (stream_) {
      FSEventStreamStop(stream_);
      FSEventStreamInvalidate(stream_);
      FSEventStreamRelease(stream_);
      stream_ = nullptr;
    }
    if (runloop_) {
      CFRunLoopStop(runloop_);
      runloop_ = nullptr;
    }
    if (thread_.joinable()) thread_.join();
  }

private:
  static void callback(ConstFSEventStreamRef,
                       void* clientCallBackInfo,
                       size_t numEvents,
                       void* eventPaths,
                       const FSEventStreamEventFlags eventFlags[],
                       const FSEventStreamEventId[]) {
    auto* self = static_cast<MacFSEventsBackend*>(clientCallBackInfo);
    CFArrayRef arr = (CFArrayRef)eventPaths;
    
    std::cout << "[FSEvents] Received " << numEvents << " events from macOS" << std::endl;

    for (size_t i = 0; i < numEvents; ++i) {
      CFStringRef cfPath = (CFStringRef)CFArrayGetValueAtIndex(arr, i);
      char buf[PATH_MAX];
      if (!CFStringGetCString(cfPath, buf, sizeof(buf),
                              kCFStringEncodingUTF8)) {
        continue;
      }

      FileWatchEvent ev;
      ev.path = std::filesystem::path(buf);
      ev.ts = std::chrono::system_clock::now();

      const auto f = eventFlags[i];
      
      std::cout << "[FSEvents] Processing event for path: " << buf 
                << ", flags: 0x" << std::hex << f << std::dec << std::endl;

      // Overflow / rescan hints
      if ((f & kFSEventStreamEventFlagMustScanSubDirs) ||
          (f & kFSEventStreamEventFlagEventIdsWrapped)) {
        ev.kind = EventKind::Overflow;
        self->handler_(ev);
        continue;
      }

      ev.is_dir = (f & kFSEventStreamEventFlagItemIsDir) != 0;

      if (f & kFSEventStreamEventFlagItemRemoved) {
        ev.kind = EventKind::Deleted;
      } else if (f & kFSEventStreamEventFlagItemCreated) {
        ev.kind = EventKind::Created;
      } else if (f & kFSEventStreamEventFlagItemRenamed) {
        ev.kind = EventKind::Renamed;
        // FSEvents doesn't provide old_path in this callback; left empty.
      } else if ((f & kFSEventStreamEventFlagItemModified) ||
                 (f & kFSEventStreamEventFlagItemChangeOwner) ||
                 (f & kFSEventStreamEventFlagItemXattrMod) ||
                 (f & kFSEventStreamEventFlagItemInodeMetaMod)) {
        ev.kind = EventKind::Modified;
      } else {
        // Default to Modified when in doubt
        ev.kind = EventKind::Modified;
      }

      std::cout << "[FSEvents] Sending event to FileWatcher: kind=" << static_cast<int>(ev.kind) 
                << ", is_dir=" << ev.is_dir << ", path=" << ev.path.string() << std::endl;
      
      self->handler_(ev);
    }
  }

  void run() {
    CFStringRef cfPath = CFStringCreateWithCString(
        nullptr, root_.string().c_str(), kCFStringEncodingUTF8);

    CFArrayRef paths = CFArrayCreate(nullptr, (const void**)&cfPath, 1,
                                     &kCFTypeArrayCallBacks);

    FSEventStreamContext ctx{};
    ctx.info = this;

    FSEventStreamCreateFlags flags =
        kFSEventStreamCreateFlagFileEvents |       // per-file events
        kFSEventStreamCreateFlagNoDefer |          // lower latency
        kFSEventStreamCreateFlagUseCFTypes |       // CFArray in callback
        kFSEventStreamCreateFlagIgnoreSelf |       // ignore our own writes
        kFSEventStreamCreateFlagWatchRoot;         // detect root replaced

    FSEventStreamEventId since = kFSEventStreamEventIdSinceNow;
    CFTimeInterval latency = 0.5;  // seconds; tradeoff spam vs latency

    stream_ = FSEventStreamCreate(nullptr, &MacFSEventsBackend::callback, &ctx,
                                  paths, since, latency, flags);

    CFRelease(paths);
    CFRelease(cfPath);

    if (!stream_) {
      // Nothing to do; stop start() thread
      return;
    }

    runloop_ = CFRunLoopGetCurrent();
    FSEventStreamScheduleWithRunLoop(stream_, runloop_, kCFRunLoopDefaultMode);
    FSEventStreamStart(stream_);

    // Block here until stop() calls CFRunLoopStop()
    CFRunLoopRun();

    // Cleanup path if loop was stopped without explicit stop()
    if (stream_) {
      FSEventStreamStop(stream_);
      FSEventStreamInvalidate(stream_);
      FSEventStreamRelease(stream_);
      stream_ = nullptr;
    }
  }

private:
  std::filesystem::path root_;
  Handler handler_;
  std::atomic<bool> running_{false};
  std::thread thread_;
  CFRunLoopRef runloop_ = nullptr;
  FSEventStreamRef stream_ = nullptr;
};

// Factory used by the core .cpp
std::unique_ptr<IFileWatcherBackend> make_mac_fsevents_backend(
    const std::filesystem::path& root, IFileWatcherBackend::Handler handler) {
  return std::make_unique<MacFSEventsBackend>(root, std::move(handler));
}

}  // namespace magic_core::async