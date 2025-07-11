# Magic Core Library
set(MAGIC_CORE_SOURCES
    ollama_client.cpp
    metadata_store.cpp
    file_watcher.cpp
    content_extractor.cpp
    types.cpp
)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
# Create the library
add_library(magic_core STATIC ${MAGIC_CORE_SOURCES})
# Link libraries
target_link_libraries(magic_core
    PUBLIC
        faiss
        LAPACK::LAPACK
        OpenSSL::SSL
        OpenSSL::Crypto
        "-framework Accelerate"
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        Threads::Threads
        unofficial::sqlite3::sqlite3
)

# Include directories
target_include_directories(magic_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Compiler flags
target_compile_features(magic_core PUBLIC cxx_std_20)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(magic_core PRIVATE inotify)
elseif(APPLE)
    # macOS specific libraries if needed
    target_link_libraries(magic_core PRIVATE
        "-framework CoreFoundation"
        "-framework CoreServices"
    )
elseif(WIN32)
    # Windows specific libraries if needed
endif() 