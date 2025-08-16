#pragma once

#include "magic_core/async/ITask.hpp"
#include <vector>
#include <chrono> 
#include <optional>
#include "magic_core/types/chunk.hpp"
namespace magic_core {
    class MetadataStore;
}
namespace magic_core {
class ProcessFileTask : public ITask {
public:
    // The constructor enforces that a file path MUST be provided.
    ProcessFileTask(long long id, TaskStatus status,
                    std::chrono::system_clock::time_point created_at,
                    std::chrono::system_clock::time_point updated_at,
                    std::optional<std::string> error_message,
                    std::string file_path);

    void execute(ServiceProvider& services, const ProgressUpdater& on_progress) override;
    const char* get_type() const override { return "PROCESS_FILE"; }

    // Public getter for its specific argument
    const std::string& get_file_path() const { return file_path_; }

private:
    void process_chunks_in_batches(long long file_id, std::vector<Chunk>& chunks, ServiceProvider& services, const ProgressUpdater& on_progress);
    void finalize_document_embedding(long long file_id, const std::vector<Chunk>& chunks, MetadataStore& store);

    std::string file_path_;
};
}  // namespace magic_core