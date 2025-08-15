#include "magic_core/async/ITask.hpp"

namespace magic_core {
class ProcessFileTask : public ITask {
public:
    // The constructor enforces that a file path MUST be provided.
    ProcessFileTask(long long id, TaskStatus status,
                    std::chrono::system_clock::time_point created_at,
                    std::string file_path);

    void execute(ServiceProvider& services, const ProgressUpdater& on_progress) override;
    const char* get_type() const override { return "PROCESS_FILE"; }

    const std::string& get_file_path() const { return file_path_; }

private:
    std::string file_path_;
};
}  // namespace magic_core