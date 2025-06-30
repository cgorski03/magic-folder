# Magic API Executable
set(MAGIC_API_SOURCES
    main.cpp
    server.cpp
    routes.cpp
)

# Create the executable
add_executable(magic_api ${MAGIC_API_SOURCES})

# Try to find Crow with more explicit configuration
find_package(Crow REQUIRED)


target_link_libraries(magic_api
    magic_core
    Crow::Crow
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# Compiler flags
target_compile_features(magic_api PRIVATE cxx_std_20)
