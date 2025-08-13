# API layer unit tests (Config)

set(API_TEST_SOURCES
    config_test.cpp
)

add_library(magic_test_api STATIC ${API_TEST_SOURCES})

target_link_libraries(magic_test_api
    PUBLIC
        magic_test_common
        magic_core
        GTest::gtest
        GTest::gmock
)

target_include_directories(magic_test_api
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/common
)

target_compile_features(magic_test_api PUBLIC cxx_std_20)


