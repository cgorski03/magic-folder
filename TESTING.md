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

### Why Integration Tests?

1. **Simple Architecture**: `FileInfoService` is a thin wrapper around `MetadataStore`
2. **Real World Testing**: Tests the actual integration between components
3. **Database Operations**: Ensures SQLite operations work correctly
4. **Vector Embeddings**: Tests the full pipeline including Faiss index operations

## Test Data

Tests use realistic file metadata including:
- Various file types (text files, images)
- Real timestamps
- Content hashes
- Vector embeddings (768-dimension for compatibility with the embedding model)

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
   Solution: Ensure vector embeddings are exactly 768 dimensions
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