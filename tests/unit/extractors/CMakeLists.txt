# Content extractor unit tests
# Tests for all content extractor classes

# Define extractor test sources
set(EXTRACTORS_TEST_SOURCES
    content_extractor_test.cpp
    markdown_extractor_test.cpp
    plaintext_extractor_test.cpp
    content_extractor_factory_test.cpp
)

# Create extractors test library
add_library(magic_test_extractors STATIC ${EXTRACTORS_TEST_SOURCES})

# Link against common test utilities and required libraries
target_link_libraries(magic_test_extractors
    PUBLIC
        magic_test_common
        magic_core
        GTest::gtest
        GTest::gmock
)

# Set include directories
target_include_directories(magic_test_extractors
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/common
)

# Set C++ standard
target_compile_features(magic_test_extractors PUBLIC cxx_std_20)

# Define individual test targets for extractors
add_custom_target(test_extractors
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*ExtractorTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all extractor tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_content_extractor
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="ContentExtractorTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running ContentExtractor tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_markdown_extractor
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="MarkdownExtractorTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running MarkdownExtractor tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_plaintext_extractor
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="PlainTextExtractorTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running PlainTextExtractor tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_content_extractor_factory
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="ContentExtractorFactoryTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running ContentExtractorFactory tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
) 