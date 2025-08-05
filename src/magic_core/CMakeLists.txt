# Magic Core Library (including all subdirectories)

# Root level sources
set(MAGIC_CORE_SOURCES
    types.cpp
)

# Automatically find all source files in subdirectories
file(GLOB_RECURSE SERVICES_SOURCES "services/*.cpp")
file(GLOB_RECURSE EXTRACTORS_SOURCES "extractors/*.cpp") 
file(GLOB_RECURSE LLM_SOURCES "llm/*.cpp")
file(GLOB_RECURSE DB_SOURCES "db/*.cpp")
file(GLOB_RECURSE ASYNC_SOURCES "async/*.cpp")

# Combine all sources
list(APPEND MAGIC_CORE_SOURCES 
    ${SERVICES_SOURCES}
    ${EXTRACTORS_SOURCES}
    ${LLM_SOURCES}
    ${DB_SOURCES}
    ${ASYNC_SOURCES}
)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(OpenMP REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
# Create the library
add_library(magic_core STATIC ${MAGIC_CORE_SOURCES})

# Link libraries
target_link_libraries(magic_core
    PUBLIC
    LAPACK::LAPACK
    OpenSSL::SSL
    OpenSSL::Crypto
    "-framework Accelerate"
    faiss
    nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
        Threads::Threads
        unofficial::sqlite3::sqlite3
        OpenMP::OpenMP_CXX
        utf8cpp::utf8cpp
        zstd::libzstd
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