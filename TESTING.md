# Testing Magic Folder

This document describes how to build and run tests for the Magic Folder C++ implementation.

## Prerequisites

### Required Dependencies

1. **vcpkg** - Package manager for C++ dependencies
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   ./bootstrap-vcpkg.sh  # macOS/Linux
   export VCPKG_ROOT=$(pwd)
   ```

2. **Google Test** - Testing framework (installed via vcpkg)
   ```bash
   # Automatically installed when running tests with --feature testing
   ```

## Building and Running Tests

### Method 1: Using the Test Runner Script (Recommended)

The easiest way to run tests is using the provided test runner script:

```bash
# Make sure VCPKG_ROOT is set
export VCPKG_ROOT=/path/to/your/vcpkg

# Run all tests
./run_tests.sh
```

This script will:
1. Install required dependencies (including Google Test)
2. Configure CMake with testing enabled
3. Build the project
4. Run all tests

### Method 2: Manual Build and Test

If you prefer to run tests manually:

```bash
# 1. Install dependencies with testing feature
cd build
$VCPKG_ROOT/vcpkg install --feature-flags=manifests,versions --feature testing

# 2. Configure CMake
cmake -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DCMAKE_BUILD_TYPE=Debug \
      ..

# 3. Build the project
make -j$(nproc)

# 4. Run all tests
./bin/magic_folder_tests

# OR run specific test suites
./bin/magic_folder_tests --gtest_filter="FileInfoServiceTest.*"

# OR use CTest (which internally runs magic_folder_tests)
ctest --output-on-failure --verbose
```

## Available Tests

### Test Structure

All tests are compiled into a single executable (`magic_folder_tests`) for efficiency and better organization.
### Encryption and Database Key Tests

With database encryption enabled (SQLCipher), tests now initialize the `MetadataStore` with a deterministic 32-byte key. The base fixture `tests/common/utilities_test.hpp` constructs `MetadataStore` with a test key so all existing tests work unchanged. Additional tests verify encryption behavior:

- Open with wrong key throws: Ensures you cannot open the same DB with an incorrect key.
- Open with correct key succeeds: Confirms the same key can reopen the DB.

You can run just these tests with:

```bash
./bin/magic_folder_tests --gtest_filter="MetadataStoreTest.OpenWith*Key*"
```

Notes:
- Keys used in tests are simple repeated characters for determinism; production uses the OS keychain via `EncryptionKeyService`.
- SQLCipher accepts arbitrary passphrase strings; in production, we store a 32-byte random key.


#### FileInfoService Tests

Tests for the `FileInfoService` class, including:

- **Basic Functionality:**
  - `ListFiles_ReturnsAllFiles` - Tests listing all files
  - `ListFiles_ReturnsEmptyWhenNoFiles` - Tests empty file lists
  - `GetFileInfo_ReturnsFileWhenExists` - Tests file retrieval by path
  - `GetFileInfo_ReturnsNulloptWhenFileNotExists` - Tests handling of non-existent files

- **Vector Embedding Support:**
  - `ListFiles_PreservesVectorEmbeddings` - Tests that vector embeddings are preserved in file lists
  - `GetFileInfo_PreservesVectorEmbedding` - Tests that vector embeddings are preserved in single file retrieval

- **Edge Cases:**
  - `GetFileInfo_HandlesRelativePaths` - Tests relative path handling
  - `GetFileInfo_HandlesEmptyPath` - Tests empty path handling
  - `GetFileInfo_PreservesTimestamps` - Tests timestamp preservation
  - `GetFileInfo_ReflectsUpdates` - Tests that updates are reflected correctly

- **Integration & Performance:**
  - `ServiceProperlyDelegatesToMetadataStore` - Tests delegation to underlying store
  - `ListFiles_HandlesLargeDataset` - Tests performance with large datasets (100+ files)

## Test Architecture

The tests use **integration testing** approach rather than mocking:

- Tests use a real `MetadataStore` instance with a temporary SQLite database
- This ensures the full integration between `FileInfoService` and `MetadataStore` works correctly
- Temporary databases are created and cleaned up for each test
- Shared test utilities (`test_utilities.hpp`) provide common functionality

### Encryption Key Handling in Tests

- Test fixtures create a temporary SQLite database file and pass a fixed test key when constructing `MetadataStore`.
- Production code acquires the key from the OS secure store using `magic_core::EncryptionKeyService::get_database_key()`; tests do not call the OS keychain.
- If you add tests that construct `MetadataStore` directly, provide a key: `MetadataStore(db_path, std::string(32, 'K'))`.

### Why Integration Tests?

1. **Simple Architecture**: `FileInfoService` is a thin wrapper around `MetadataStore`
2. **Real World Testing**: Tests the actual integration between components
3. **Database Operations**: Ensures SQLite operations work correctly
4. **Vector Embeddings**: Tests the full pipeline including Faiss index operations
5. **Encryption**: Validates that encrypted databases cannot be opened with wrong keys

## Test Data

Tests use realistic file metadata including:
- Various file types (text files, images)
- Real timestamps
- Content hashes
- Vector embeddings (1024-dimension for compatibility with the embedding model)

## Debugging Tests

### Running Specific Tests

To run all tests:
```bash
cd build
./bin/magic_folder_tests
```

To run a specific test suite:
```bash
./bin/magic_folder_tests --gtest_filter="FileInfoServiceTest.*"
```

To run a specific individual test:
```bash
./bin/magic_folder_tests --gtest_filter="FileInfoServiceTest.ListFiles_ReturnsAllFiles"
```

To list all available tests:
```bash
./bin/magic_folder_tests --gtest_list_tests
```

### Verbose Output

For more detailed test output:
```bash
./bin/magic_folder_tests --gtest_output=verbose
```

### Core Worker/WorkerPool Tests

Run both worker and worker-pool tests:

```bash
./bin/magic_folder_tests --gtest_filter="*WorkerTest*:*WorkerPoolTest*"
```

Run only worker-pool tests:

```bash
./bin/magic_folder_tests --gtest_filter="WorkerPoolTest.*"
```

### Debug Build

Tests are built in Debug mode by default for better debugging:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

## Troubleshooting

### Common Issues

1. **VCPKG_ROOT not set**
   ```
   Error: VCPKG_ROOT environment variable not set
   Solution: Export VCPKG_ROOT=/path/to/vcpkg
   ```

2. **Google Test not found**
   ```
   Error: find_package(GTest) failed
   Solution: Install with testing feature: vcpkg install --feature testing
   ```

3. **SQLite database errors**
   ```
   Error: Database permission issues
   Solution: Ensure write permissions to temp directory
   ```

4. **Vector dimension mismatch**
   ```
   Error: Vector embedding size mismatch
   Solution: Ensure vector embeddings are exactly 1024 dimensions
   ```

## Adding New Tests

To add new tests to the test suite:

1. **Add test file**: Create `test_new_component.cpp` in the `tests/` directory
2. **Update CMakeLists.txt**: Add the new file to the `TEST_SOURCES` list
3. **Use shared utilities**: Leverage `test_utilities.hpp` and base classes
4. **Follow naming conventions**: Use `ComponentNameTest` for test class names
5. **Run tests**: Ensure all tests pass before submitting

### Adding a New Test File

1. Create the test file (e.g., `test_metadata_store.cpp`):
   ```cpp
   #include <gtest/gtest.h>
   #include "test_utilities.hpp"
   #include "magic_core/metadata_store.hpp"
   
   class MetadataStoreTest : public magic_tests::MetadataStoreTestBase {
       // Your tests here
   };
   ```

2. Add to `CMakeLists.txt`:
   ```cmake
   set(TEST_SOURCES
       test_main.cpp
       test_utilities.cpp
       test_file_info_service.cpp
       test_metadata_store.cpp  # Add your new file here
   )
   ```

3. Optionally add a convenience target:
   ```cmake
   add_custom_target(test_metadata_store
       COMMAND magic_folder_tests --gtest_filter="MetadataStoreTest.*"
       DEPENDS magic_folder_tests
       COMMENT "Running MetadataStore tests only"
   )
   ```

Example test structure:
```cpp
#include <gtest/gtest.h>
#include "your_component.hpp"

class YourComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(YourComponentTest, TestName) {
    // Arrange
    // Act  
    // Assert
}
```

# Magic Folder Testing Guide

## Overview

The Magic Folder project uses Google Test framework for unit testing with comprehensive mocking support for external dependencies. The test suite is organized to provide isolated testing of individual components while maintaining realistic integration scenarios.

## Test Structure

### Test Organization

```
tests/
├── test_main.cpp              # Main test entry point
├── test_utilities.hpp         # Common test utilities and base classes
├── test_utilities.cpp         # Implementation of test utilities
├── test_mocks.hpp            # Mock classes for external dependencies
├── test_file_info_service.cpp # FileInfoService tests
├── test_file_delete_service.cpp # FileDeleteService tests
├── test_file_processing_service.cpp # FileProcessingService tests
└── CMakeLists.txt            # Test build configuration
```

### Test Base Classes

#### `MetadataStoreTestBase`
- Provides a temporary database for each test
- Automatically cleans up after each test
- Inherits from `::testing::Test`
- Used by services that require a metadata store

#### `TestUtilities`
- Static utility methods for creating test data
- Database management utilities
- Test file metadata creation helpers

## Mocking Strategy

### Mock Classes (`test_mocks.hpp`)

The project uses Google Mock (gmock) to create mock implementations of external dependencies:

#### `MockOllamaClient`
```cpp
class MockOllamaClient : public magic_core::OllamaClient {
 public:
  MockOllamaClient() : magic_core::OllamaClient("http://localhost:11434", "nomic-embed-text") {}
  
  MOCK_METHOD(std::vector<float>, get_embedding, (const std::string& text), (override));
};
```

**Purpose**: Mocks the Ollama embedding service to avoid network calls during testing.

**Key Methods**:
- `get_embedding()` - Returns mock embedding vectors for text content

#### `MockContentExtractor`
```cpp
class MockContentExtractor : public magic_core::ContentExtractor {
 public:
  MockContentExtractor() : magic_core::ContentExtractor() {}
  
  MOCK_METHOD(magic_core::ExtractedContent, extract_content, (const std::filesystem::path& file_path), (override));
  MOCK_METHOD(magic_core::ExtractedContent, extract_from_text, (const std::string& text, const std::string& filename), (override));
  MOCK_METHOD(magic_core::FileType, detect_file_type, (const std::filesystem::path& file_path), (override));
  MOCK_METHOD(bool, is_supported_file_type, (const std::filesystem::path& file_path), (override));
  MOCK_METHOD(std::vector<std::string>, get_supported_extensions, (), (const, override));
};
```

**Purpose**: Mocks the content extraction service to avoid file system dependencies and complex text processing during testing.

**Key Methods**:
- `extract_content()` - Returns mock extracted content for file paths
- `extract_from_text()` - Returns mock extracted content for text input
- `detect_file_type()` - Returns mock file type detection
- `is_supported_file_type()` - Returns mock file type support check

### Mock Utilities (`MockUtilities`)

Provides helper functions for creating consistent test data:

```cpp
namespace MockUtilities {
  // Create test ExtractedContent with customizable parameters
  magic_core::ExtractedContent create_test_extracted_content(
      const std::string& text_content = "This is test content for processing.",
      const std::string& title = "Test File",
      magic_core::FileType file_type = magic_core::FileType::Text,
      const std::string& content_hash = "test_hash_123");

  // Create test embedding vectors
  std::vector<float> create_test_embedding(float value = 0.1f, size_t dimensions = 1024);
  
  // Create embeddings with custom values
  std::vector<float> create_test_embedding_with_values(const std::vector<float>& values);
}
```

## FileProcessingService Testing

### Test Structure

The `FileProcessingServiceTest` class demonstrates comprehensive mocking and testing patterns:

```cpp
class FileProcessingServiceTest : public magic_tests::MetadataStoreTestBase {
 protected:
  void SetUp() override {
    MetadataStoreTestBase::SetUp();
    
    // Create mock dependencies
    mock_content_extractor_ = std::make_shared<magic_tests::MockContentExtractor>();
    mock_ollama_client_ = std::make_shared<magic_tests::MockOllamaClient>();
    
    // Create service with mocked dependencies
    file_processing_service_ = std::make_unique<FileProcessingService>(
        metadata_store_, mock_content_extractor_, mock_ollama_client_);
    
    setupTestFile();
  }
  
  // Helper methods for test setup
  void setupTestFile();
  void setupSuccessfulProcessingExpectations(const magic_core::ExtractedContent& content,
                                           const std::vector<float>& embedding);
};
```

### Test Categories

#### 1. Successful Processing Tests
- **Purpose**: Verify normal operation with valid inputs
- **Mock Setup**: Both content extraction and embedding generation succeed
- **Assertions**: Check that metadata is properly stored with correct values

#### 2. File Type Tests
- **Purpose**: Verify handling of different file types (Text, Markdown, Code)
- **Mock Setup**: Different `ExtractedContent` with varying file types
- **Assertions**: Verify file type is correctly stored

#### 3. Error Handling Tests
- **Purpose**: Verify graceful handling of failures
- **Mock Setup**: Mock methods throw exceptions
- **Assertions**: Verify exceptions are properly propagated

#### 4. Edge Case Tests
- **Purpose**: Test boundary conditions and unusual inputs
- **Mock Setup**: Empty content, large content, special characters, unicode
- **Assertions**: Verify robust handling of edge cases

#### 5. Integration Tests
- **Purpose**: Verify service properly integrates with metadata store
- **Mock Setup**: Multiple processing calls with different content
- **Assertions**: Verify updates and persistence

### Mock Expectations

#### Setting Up Expectations
```cpp
// Expect content extraction to be called with specific file path
EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
    .WillOnce(testing::Return(extracted_content));

// Expect embedding generation to be called with extracted text
EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
    .WillOnce(testing::Return(embedding));
```

#### Error Simulation
```cpp
// Simulate content extraction failure
EXPECT_CALL(*mock_content_extractor_, extract_content(test_file_path_))
    .WillOnce(testing::Throw(magic_core::ContentExtractorError("Extraction failed")));

// Simulate embedding generation failure
EXPECT_CALL(*mock_ollama_client_, get_embedding(extracted_content.text_content))
    .WillOnce(testing::Throw(magic_core::OllamaError("Embedding failed")));
```

## Running Tests

### Build and Run All Tests
```bash
./run_tests.sh
```

### Run Specific Test Suites
```bash
# FileInfoService tests
GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_filter="FileInfoServiceTest.*" --gtest_color=yes

# FileDeleteService tests
GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_filter="FileDeleteServiceTest.*" --gtest_color=yes

# FileProcessingService tests
GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_filter="FileProcessingServiceTest.*" --gtest_color=yes
```

### Using CMake Targets
```bash
# Build and run specific test suites
make test_file_info_service
make test_file_delete_service
make test_file_processing_service
```

## Test Data Management

### Temporary Files
- Tests create temporary files in system temp directory
- Files are automatically cleaned up in `TearDown()`
- File paths are unique to avoid conflicts

### Database Management
- Each test gets a fresh temporary database
- Databases are automatically cleaned up after tests
- No test data persists between test runs

### Mock Data Consistency
- Mock utilities provide consistent test data
- Embedding vectors are deterministic for testing
- File metadata follows consistent patterns

## Best Practices

### Mock Usage
1. **Isolate Dependencies**: Mock external services to avoid network/file system dependencies
2. **Consistent Data**: Use mock utilities for consistent test data creation
3. **Clear Expectations**: Set up explicit mock expectations for each test
4. **Error Simulation**: Test error conditions by making mocks throw exceptions

### Test Organization
1. **Arrange-Act-Assert**: Follow clear test structure
2. **Helper Methods**: Extract common setup into helper methods
3. **Descriptive Names**: Use descriptive test names that explain the scenario
4. **Independent Tests**: Each test should be independent and not rely on other tests

### Assertions
1. **Comprehensive Checks**: Verify all relevant aspects of the result
2. **Edge Cases**: Test boundary conditions and error scenarios
3. **Integration**: Verify that components work together correctly
4. **Performance**: Test with realistic data sizes when appropriate

## Adding New Tests

### For New Services
1. Create test file following naming convention: `test_<service_name>.cpp`
2. Inherit from appropriate base class (`MetadataStoreTestBase` if needed)
3. Add mock dependencies using `test_mocks.hpp`
4. Add test file to `tests/CMakeLists.txt`
5. Add custom target for running specific tests

### For New Mock Classes
1. Add mock class to `test_mocks.hpp`
2. Include appropriate MOCK_METHOD declarations
3. Add utility functions to `MockUtilities` namespace if needed
4. Update documentation

### For New Test Utilities
1. Add utility functions to `TestUtilities` class in `test_utilities.hpp`
2. Implement in `test_utilities.cpp`
3. Document usage and purpose
4. Add to this documentation

## Troubleshooting

### Common Issues
1. **Mock Not Called**: Ensure mock expectations are set up before the action
2. **Test Data Conflicts**: Use unique identifiers for test data
3. **Cleanup Failures**: Ensure all temporary files are properly cleaned up
4. **Build Errors**: Check that all test files are included in CMakeLists.txt

### Debugging Tips
1. Use `--gtest_output=verbose` for detailed test output
2. Use `--gtest_filter` to run specific tests
3. Check mock expectations with `EXPECT_CALL` debugging
4. Use temporary file logging to debug file system issues 