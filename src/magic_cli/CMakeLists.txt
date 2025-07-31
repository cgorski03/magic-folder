# Magic CLI Executable
set(MAGIC_CLI_SOURCES
    main.cpp
    cli_handler.cpp
)

# Create the executable
add_executable(magic_cli ${MAGIC_CLI_SOURCES})

# Link libraries
target_link_libraries(magic_cli
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# Include directories
target_include_directories(magic_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Compiler flags
target_compile_features(magic_cli PRIVATE cxx_std_20)

# Note: For CLI parsing, you'll need to add a library like CLI11 or cxxopts
# For now, we'll use a simple argument parsing implementation 