#include "utilities_test.hpp"

#include <chrono>
#include <filesystem>

namespace magic_tests {

std::filesystem::path TestUtilities::create_temp_test_db() {
  auto temp_dir = std::filesystem::temp_directory_path() / "magic_folder_tests";
  std::filesystem::create_directories(temp_dir);

  // Generate unique filename using timestamp
  auto now = std::chrono::system_clock::now();
  auto timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

  return temp_dir / ("test_" + std::to_string(timestamp) + ".db");
}

void TestUtilities::cleanup_temp_db(const std::filesystem::path& db_path) {
  if (std::filesystem::exists(db_path)) {
    std::filesystem::remove(db_path);
  }

  // Also cleanup the parent directory if it's empty
  auto parent_dir = db_path.parent_path();
  if (std::filesystem::exists(parent_dir) && std::filesystem::is_empty(parent_dir)) {
    std::filesystem::remove(parent_dir);
  }
}

magic_core::BasicFileMetadata TestUtilities::create_test_basic_file_metadata(
    const std::string& path,
    const std::string& content_hash,
    magic_core::FileType file_type,
    size_t file_size,
    const std::string& processing_status,
    const std::string& original_path,
    const std::string& tags) {
  
  magic_core::BasicFileMetadata metadata;
  metadata.path = path;
  metadata.original_path = original_path.empty() ? path : original_path;
  metadata.file_hash = content_hash;
  metadata.file_type = file_type;
  metadata.file_size = file_size;
  metadata.processing_status = processing_status;
  metadata.tags = tags;

  auto now = std::chrono::system_clock::now();
  metadata.last_modified = now;
  metadata.created_at = now - std::chrono::hours(1);  // Created 1 hour ago

  return metadata;
}

magic_core::FileMetadata TestUtilities::create_test_file_metadata(
    const std::string& path,
    const std::string& content_hash,
    magic_core::FileType file_type,
    size_t file_size,
    bool include_vector,
    const std::string& processing_status,
    const std::string& original_path,
    const std::string& tags,
    const std::string& suggested_category,
    const std::string& suggested_filename) {
  
  magic_core::FileMetadata metadata;
  metadata.path = path;
  metadata.original_path = original_path.empty() ? path : original_path;
  metadata.file_hash = content_hash;
  metadata.file_type = file_type;
  metadata.file_size = file_size;
  metadata.processing_status = processing_status;
  metadata.tags = tags;
  metadata.suggested_category = suggested_category;
  metadata.suggested_filename = suggested_filename;

  auto now = std::chrono::system_clock::now();
  metadata.last_modified = now;
  metadata.created_at = now - std::chrono::hours(1);  // Created 1 hour ago

  if (include_vector) {
    metadata.summary_vector_embedding = create_test_vector(path, 1024);
  }

  return metadata;
}

std::vector<magic_core::FileMetadata> TestUtilities::create_test_dataset(int count,
                                                                         const std::string& prefix,
                                                                         bool include_vectors) {
  std::vector<magic_core::FileMetadata> dataset;
  dataset.reserve(count);

  for (int i = 0; i < count; ++i) {
    std::string path = prefix + "/file" + std::to_string(i) + ".txt";
    std::string hash = "hash" + std::to_string(i);

    dataset.push_back(create_test_file_metadata(path, hash, magic_core::FileType::Text, 1024 + i,
                                                include_vectors));
  }

  return dataset;
}

std::vector<float> TestUtilities::create_test_vector(const std::string& seed_text, int dimension) {
  std::vector<float> vector;
  vector.resize(dimension);
  
  std::hash<std::string> hasher;
  size_t seed_hash = hasher(seed_text);

  for (int i = 0; i < dimension; ++i) {
    vector[i] = static_cast<float>((seed_hash + i) % 1000) / 1000.0f;
  }

  return vector;
}

Chunk TestUtilities::create_test_chunk_with_embedding(
    const std::string& content, int chunk_index, const std::string& seed_text) {
  
  Chunk chunk;
  chunk.content = content;
  chunk.chunk_index = chunk_index;
  chunk.vector_embedding = create_test_vector(seed_text + "_chunk_" + std::to_string(chunk_index), 1024);
  
  return chunk;
}

std::vector<Chunk> TestUtilities::create_test_chunks(
    int count, const std::string& base_content) {
  
  std::vector<Chunk> chunks;
  chunks.reserve(count);

  for (int i = 0; i < count; ++i) {
    std::string content = base_content + " " + std::to_string(i);
    chunks.push_back(create_test_chunk_with_embedding(content, i, base_content));
  }

  return chunks;
}

void TestUtilities::populate_metadata_store_with_stubs(
    std::shared_ptr<magic_core::MetadataStore> store,
    const std::vector<magic_core::BasicFileMetadata>& files) {
  
  for (const auto& file : files) {
    store->upsert_file_stub(file);
  }
}

int TestUtilities::create_complete_file_in_store(
    std::shared_ptr<magic_core::MetadataStore> store,
    const magic_core::FileMetadata& metadata,
    const std::vector<Chunk>& chunks) {
  
  // First create the basic stub
  magic_core::BasicFileMetadata basic_metadata;
  basic_metadata.path = metadata.path;
  basic_metadata.original_path = metadata.original_path;
  basic_metadata.file_hash = metadata.file_hash;
  basic_metadata.last_modified = metadata.last_modified;
  basic_metadata.created_at = metadata.created_at;
  basic_metadata.file_type = metadata.file_type;
  basic_metadata.file_size = metadata.file_size;
  basic_metadata.processing_status = metadata.processing_status;
  basic_metadata.tags = metadata.tags;

  int file_id = store->upsert_file_stub(basic_metadata);

  // Update with AI analysis if there's a summary vector
  if (!metadata.summary_vector_embedding.empty()) {
    store->update_file_ai_analysis(file_id, metadata.summary_vector_embedding,
                                   metadata.suggested_category, metadata.suggested_filename);
  }

  // Add chunks if provided
  if (!chunks.empty()) {
    // Convert Chunk to ProcessedChunk with mock compressed content
    std::vector<magic_core::ProcessedChunk> processed_chunks;
    processed_chunks.reserve(chunks.size());
    
    for (const auto& chunk : chunks) {
      magic_core::ProcessedChunk processed_chunk;
      processed_chunk.chunk = chunk;
      // Use mock compressed content for testing (not actual compression)
      std::string mock_compressed = "compressed_" + chunk.content;
      processed_chunk.compressed_content = std::vector<char>(mock_compressed.begin(), mock_compressed.end());
      processed_chunks.push_back(std::move(processed_chunk));
    }
    
    store->upsert_chunk_metadata(file_id, processed_chunks);
  }

  return file_id;
}

}  // namespace magic_tests