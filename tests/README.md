# Magic Folder Test Suite

This directory contains the organized test suite for the Magic Folder project, structured for scalability and ease of use.

## Directory Structure

```
tests/
├── CMakeLists.txt                 # Main test configuration
├── common/                        # Shared test utilities
│   ├── CMakeLists.txt
│   ├── utilities_test.hpp         # Common test helper functions
│   ├── utilities_test.cpp
│   └── mocks_test.hpp             # Mock objects for testing
├── unit/                          # Unit tests
│   ├── CMakeLists.txt             # Unit test coordination
│   ├── core/                      # Core functionality tests (future)
│   │   └── CMakeLists.txt
│   ├── services/                  # Service layer tests
│   │   ├── CMakeLists.txt
│   │   ├── compression_service_test.cpp
│   │   ├── file_processing_service_test.cpp
│   │   └── search_service_test.cpp
│   ├── extractors/                # Content extractor tests
│   │   ├── CMakeLists.txt
│   │   ├── content_extractor_test.cpp
│   │   ├── markdown_extractor_test.cpp
│   │   ├── plaintext_extractor_test.cpp
│   │   └── content_extractor_factory_test.cpp
│   └── db/                        # Database layer tests
│       ├── CMakeLists.txt
│       ├── metadata_store_test.cpp
│       ├── file_info_service_test.cpp
│       └── file_delete_service_test.cpp
├── integration/                   # Integration tests (future)
│   └── CMakeLists.txt
└── README.md                      # This file
```

## Available Test Targets

### High-Level Targets

- **`test_all`** - Run all tests
- **`test_unit_all`** - Run all unit tests (excluding integration tests)
- **`test_help`** - Display all available test targets

### Layer-Specific Targets

- **`test_services_all`** - Run all service layer tests
- **`test_extractors_all`** - Run all extractor tests  
- **`test_db_all`** - Run all database layer tests

### Individual Test Targets

#### Service Tests
- **`test_compression_service`** - CompressionService tests (27 tests)
- **`test_file_processing_service`** - FileProcessingService tests (11 tests)
- **`test_search_service`** - SearchService tests (26 tests, 10 currently failing)

#### Extractor Tests
- **`test_content_extractor`** - ContentExtractor tests
- **`test_markdown_extractor`** - MarkdownExtractor tests
- **`test_plaintext_extractor`** - PlainTextExtractor tests
- **`test_content_extractor_factory`** - ContentExtractorFactory tests

#### Database Tests
- **`test_metadata_store`** - MetadataStore tests
- **`test_file_info_service`** - FileInfoService tests
- **`test_file_delete_service`** - FileDeleteService tests

## Usage

```bash
# Build and run all tests
make test_all

# Run specific test categories
make test_services_all
make test_extractors_all
make test_db_all

# Run individual test suites
make test_compression_service
make test_metadata_store

# See all available targets
make test_help
```

## Test Libraries

The modular structure creates several CMake libraries:

- **`magic_test_common`** - Shared test utilities and mocks
- **`magic_test_services`** - Service layer test library
- **`magic_test_extractors`** - Extractor test library
- **`magic_test_db`** - Database layer test library

## Current Test Status

As of the latest refactoring:

-  **179/189 tests passing** (94.7% pass rate)
-  **CompressionService**: 27/27 tests passing
-  **FileProcessingService**: 11/11 tests passing
-  **SearchService**: 16/26 tests passing (10 failing due to mock compression format)
-  **All Extractor tests**: Passing
-  **All Database tests**: Passing

## Key Features

### Modular Organization
- Tests are organized by architectural layer (services, extractors, db)
- Shared utilities are centralized in the `common/` directory
- Each module has its own CMakeLists.txt for maintainability

### Scalable Targets
- High-level targets for running entire test categories
- Individual targets for focused development
- Helpful target discovery with `test_help`

### Clean Dependencies
- Common test utilities are built as a separate library
- Clear dependency hierarchy prevents circular dependencies
- Easy to add new test modules

### Developer-Friendly
- Fast iteration with individual test targets
- Clear naming conventions
- Comprehensive help system

## Adding New Tests

### Adding a Test to an Existing Module

1. Add your test file to the appropriate directory (e.g., `unit/services/`)
2. Update the corresponding `CMakeLists.txt` to include the new source file
3. Add a new target if needed for the specific test suite

### Adding a New Test Module

1. Create a new directory under `unit/` (e.g., `unit/api/`)
2. Create a `CMakeLists.txt` following the existing patterns
3. Update `unit/CMakeLists.txt` to include the new subdirectory
4. Add appropriate targets to the main `tests/CMakeLists.txt`

## Future Enhancements

- **Integration Tests**: The `integration/` directory is ready for end-to-end tests
- **Performance Tests**: Could add a `performance/` directory for benchmarking
- **API Tests**: Could add `unit/api/` for HTTP endpoint testing
- **Test Parallelization**: Could add parallel test execution for faster CI

## Dependencies

The test suite requires:
- Google Test (GTest) - Testing framework
- Google Mock (GMock) - Mocking framework  
- utf8cpp - UTF-8 string handling
- magic_core - The main application library