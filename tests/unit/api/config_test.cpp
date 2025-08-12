#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdio>

#include "magic_api/config.hpp"

namespace {

std::string write_temp_file(const std::string& contents) {
  char filename_template[] = "/tmp/magic_folder_config_test_XXXXXX.json";
  int fd = mkstemps(filename_template, 5); // 5 for ".json"
  if (fd == -1) {
    throw std::runtime_error("Failed to create temporary file");
  }
  FILE* file = fdopen(fd, "w");
  if (!file) {
    close(fd);
    throw std::runtime_error("Failed to open temporary file stream");
  }
  fwrite(contents.data(), 1, contents.size(), file);
  fclose(file);
  return std::string(filename_template);
}

void remove_file(const std::string& path) {
  std::remove(path.c_str());
}

} // namespace

TEST(ConfigTest, LoadsFromJsonWithDefaults) {
  nlohmann::json j = {
      {"api_base_url", "0.0.0.0:8080"},
      {"metadata_db_path", "./data/meta.db"},
      {"ollama_url", "http://localhost:11434"},
      {"embedding_model", "mxbai-embed-large"},
      {"num_workers", 4}
  };

  Config cfg = Config::from_json(j);

  EXPECT_EQ(cfg.api_base_url, "0.0.0.0:8080");
  EXPECT_EQ(cfg.metadata_db_path, "./data/meta.db");
  EXPECT_EQ(cfg.ollama_url, "http://localhost:11434");
  EXPECT_EQ(cfg.embedding_model, "mxbai-embed-large");
  EXPECT_EQ(cfg.num_workers, 4);
}

TEST(ConfigTest, AppliesDefaultsWhenMissing) {
  nlohmann::json j = nlohmann::json::object();

  Config cfg = Config::from_json(j);

  EXPECT_EQ(cfg.api_base_url, "127.0.0.1:3030");
  EXPECT_EQ(cfg.metadata_db_path, "./data/metadata.db");
  EXPECT_EQ(cfg.ollama_url, "http://localhost:11434");
  EXPECT_EQ(cfg.embedding_model, "mxbai-embed-large");
  EXPECT_EQ(cfg.num_workers, 1);
}

TEST(ConfigTest, FromFileParsesAndValidates) {
  std::string contents = R"JSON({
    "api_base_url": "127.0.0.1:4000",
    "metadata_db_path": "./db/metadata.db",
    "ollama_url": "http://localhost:11434",
    "embedding_model": "mxbai-embed-large",
    "num_workers": 2
  })JSON";

  std::string path = write_temp_file(contents);
  Config cfg;
  try {
    cfg = Config::from_file(path);
  } catch (...) {
    remove_file(path);
    throw;
  }
  remove_file(path);

  EXPECT_EQ(cfg.api_base_url, "127.0.0.1:4000");
  EXPECT_EQ(cfg.metadata_db_path, "./db/metadata.db");
  EXPECT_EQ(cfg.ollama_url, "http://localhost:11434");
  EXPECT_EQ(cfg.embedding_model, "mxbai-embed-large");
  EXPECT_EQ(cfg.num_workers, 2);
}

TEST(ConfigTest, InvalidPathThrows) {
  EXPECT_THROW({
    (void)Config::from_file("/nonexistent/path/config.json");
  }, std::runtime_error);
}

TEST(ConfigTest, EmptyRequiredFieldThrows) {
  nlohmann::json j = {
      {"api_base_url", ""},
      {"metadata_db_path", "./data/meta.db"},
      {"ollama_url", "http://localhost:11434"},
      {"embedding_model", "mxbai-embed-large"},
      {"num_workers", 1}
  };

  EXPECT_THROW({ (void)Config::from_json(j); }, std::runtime_error);
}

TEST(ConfigTest, NonPositiveWorkersDefaultsAndValidates) {
  nlohmann::json j = {
      {"api_base_url", "127.0.0.1:3030"},
      {"metadata_db_path", "./data/metadata.db"},
      {"ollama_url", "http://localhost:11434"},
      {"embedding_model", "mxbai-embed-large"},
      {"num_workers", 0}
  };

  // from_json will accept 0 then validate() should throw
  EXPECT_THROW({ (void)Config::from_json(j); }, std::runtime_error);
}


