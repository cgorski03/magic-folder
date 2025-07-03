# Magic API Executable
set(MAGIC_API_SOURCES
    main.cpp
    server.cpp
    routes.cpp
)

# Create the executable
add_executable(magic_api ${MAGIC_API_SOURCES})

# Find required packages
find_package(Crow REQUIRED)
find_package(OpenSSL REQUIRED)

# Link libraries
target_link_libraries(magic_api
    magic_services
    Crow::Crow
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${SQLITE_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Compiler flags
target_compile_features(magic_api PRIVATE cxx_std_20)
