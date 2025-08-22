# Magic Folder Test Suite
# Organized test structure with modular CMake configuration
cmake_minimum_required(VERSION 3.20)

# Find required packages
find_package(GTest CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)

# Include subdirectories in dependency order
add_subdirectory(common)        # Common test utilities (must be first)
add_subdirectory(unit)          # Unit tests
add_subdirectory(integration)   # Integration tests (future)

# Collect all test sources for the main executable
set(ALL_TEST_SOURCES
    test_main.cpp
    # Unit test sources
    unit/core/worker_test.cpp
    unit/core/worker_pool_test.cpp
    unit/core/task_factory_test.cpp
    unit/core/process_file_task_test.cpp
    unit/core/service_provider_test.cpp
    unit/core/file_watcher_service_test.cpp
    unit/services/compression_service_test.cpp
    unit/services/file_processing_service_test.cpp
    unit/services/search_service_test.cpp
    unit/extractors/content_extractor_test.cpp
    unit/extractors/markdown_extractor_test.cpp
    unit/extractors/plaintext_extractor_test.cpp
    unit/extractors/content_extractor_factory_test.cpp
    unit/db/metadata_store_test.cpp
    unit/db/file_info_service_test.cpp
    unit/db/file_delete_service_test.cpp
    unit/db/task_repo_test.cpp
    unit/db/connection_pool_test.cpp
    unit/db/database_manager_test.cpp
    unit/api/config_test.cpp
)

# Create the main test executable
add_executable(magic_folder_tests ${ALL_TEST_SOURCES})

# Link against all required libraries
target_link_libraries(magic_folder_tests
    PRIVATE
        magic_test_common
        magic_core
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        utf8cpp::utf8cpp
)

# Set include directories
target_include_directories(magic_folder_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/common  # For utilities_test.hpp, mocks_test.hpp
)

# Set C++ standard
target_compile_features(magic_folder_tests PUBLIC cxx_std_20)

# Register with CTest
enable_testing()
add_test(NAME MagicFolderAllTests COMMAND magic_folder_tests)

# Enable Google Test discovery
include(GoogleTest)
gtest_discover_tests(magic_folder_tests)

# =============================================================================
# HIGH-LEVEL TEST TARGETS
# =============================================================================

# Run all tests
add_custom_target(test_all
    COMMAND $<TARGET_FILE:magic_folder_tests>
    DEPENDS magic_folder_tests
    COMMENT "Running all tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Run only unit tests
add_custom_target(test_unit_all
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="-*Integration*"
    DEPENDS magic_folder_tests
    COMMENT "Running all unit tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Run tests by layer
add_custom_target(test_services_all
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*ServiceTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all service layer tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_extractors_all
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*ExtractorTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all extractor tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_db_all
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*StoreTest*:*InfoServiceTest*:*DeleteServiceTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all database layer tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_core_all
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*WorkerTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all core layer tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# =============================================================================
# INDIVIDUAL TEST TARGETS (for quick development iteration)
# =============================================================================
# Note: Individual test targets are defined in their respective subdirectories:
# - Service tests: unit/services/CMakeLists.txt
# - Extractor tests: unit/extractors/CMakeLists.txt  
# - Database tests: unit/db/CMakeLists.txt

# =============================================================================
# UTILITY TARGETS
# =============================================================================

# Show available test targets
add_custom_target(test_help
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available test targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "======================="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "High-level targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_all           - Run all tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_unit_all      - Run all unit tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Layer targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_core_all      - Run all core tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_services_all  - Run all service tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_extractors_all- Run all extractor tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_db_all        - Run all database tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Individual targets defined in subdirectories:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Core tests:"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_core             - Worker tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "  Service tests:"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_compression_service      - CompressionService tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_file_processing_service  - FileProcessingService tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_search_service           - SearchService tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "  Extractor tests:"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_content_extractor         - ContentExtractor tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_markdown_extractor        - MarkdownExtractor tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_plaintext_extractor       - PlainTextExtractor tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_content_extractor_factory - ContentExtractorFactory tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "  Database tests:"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_metadata_store     - MetadataStore tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_file_info_service  - FileInfoService tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    test_file_delete_service- FileDeleteService tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage: make target_name"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMENT "Showing available test targets"
)