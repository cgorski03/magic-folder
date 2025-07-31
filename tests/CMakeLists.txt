# Tests configuration
cmake_minimum_required(VERSION 3.20)

# Find Google Test
find_package(GTest CONFIG REQUIRED)

# Define all test source files
set(TEST_SOURCES
    test_main.cpp
    test_utilities.cpp
    db/test_file_info_service.cpp
    db/test_file_delete_service.cpp
    db/test_metadata_store.cpp
    extractors/test_content_extractor.cpp
    extractors/test_markdown_extractor.cpp
    extractors/test_plaintext_extractor.cpp
    extractors/test_content_extractor_factory.cpp
    test_file_processing_service.cpp
    test_search_service.cpp
)

# Create single test executable for all tests
add_executable(magic_folder_tests ${TEST_SOURCES})

# Find utf8cpp for tests
find_package(utf8cpp CONFIG REQUIRED)

# Link against required libraries
target_link_libraries(magic_folder_tests 
    PRIVATE
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
    ${CMAKE_CURRENT_SOURCE_DIR}  # For test_utilities.hpp
)

# Add test to CTest
add_test(NAME MagicFolderAllTests COMMAND magic_folder_tests)

# Enable testing features
include(GoogleTest)
gtest_discover_tests(magic_folder_tests)

# Optional: Create convenience targets for running specific test suites
add_custom_target(test_file_info_service
    COMMAND magic_folder_tests --gtest_filter="FileInfoServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileInfoService tests only"
)

add_custom_target(test_file_delete_service
    COMMAND magic_folder_tests --gtest_filter="FileDeleteServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileDeleteService tests only"
)

add_custom_target(test_metadata_store
    COMMAND magic_folder_tests --gtest_filter="MetadataStoreTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running MetadataStore tests only"
)

add_custom_target(test_extractors
    COMMAND magic_folder_tests --gtest_filter="*ExtractorTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all extractor tests"
)

add_custom_target(test_content_extractor
    COMMAND magic_folder_tests --gtest_filter="ContentExtractorTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running ContentExtractor tests only"
)

add_custom_target(test_markdown_extractor
    COMMAND magic_folder_tests --gtest_filter="MarkdownExtractorTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running MarkdownExtractor tests only"
)

add_custom_target(test_plaintext_extractor
    COMMAND magic_folder_tests --gtest_filter="PlainTextExtractorTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running PlainTextExtractor tests only"
)

add_custom_target(test_content_extractor_factory
    COMMAND magic_folder_tests --gtest_filter="ContentExtractorFactoryTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running ContentExtractorFactory tests only"
)

add_custom_target(test_file_processing_service
    COMMAND magic_folder_tests --gtest_filter="FileProcessingServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileProcessingService tests only"
)

# Add convenience target for search service tests
add_custom_target(test_search_service
    COMMAND magic_folder_tests --gtest_filter="SearchServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running SearchService tests only"
)