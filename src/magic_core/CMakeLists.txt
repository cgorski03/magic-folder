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

# Handle macOS-specific Objective-C++ files
if(APPLE)
    file(GLOB_RECURSE ASYNC_MM_SOURCES "async/*.mm")
    list(APPEND ASYNC_SOURCES ${ASYNC_MM_SOURCES})
    # Enable Objective-C++ language
    enable_language(OBJCXX)
endif()

# Combine all sources
list(APPEND MAGIC_CORE_SOURCES 
    ${SERVICES_SOURCES}
    ${EXTRACTORS_SOURCES}
    ${LLM_SOURCES}
    ${DB_SOURCES}
    ${ASYNC_SOURCES}
)

find_path(SQLITE_MODERN_CPP_INCLUDE_DIRS "sqlite_modern_cpp.h")

# Find SQLCipher installed via Homebrew
find_path(SQLCIPHER_INCLUDE_DIR sqlcipher/sqlite3.h
    PATHS /opt/homebrew/include
    NO_DEFAULT_PATH
)

find_library(SQLCIPHER_LIBRARY 
    NAMES sqlcipher
    PATHS /opt/homebrew/lib
    NO_DEFAULT_PATH
)

if(NOT SQLCIPHER_INCLUDE_DIR OR NOT SQLCIPHER_LIBRARY)
    message(FATAL_ERROR "SQLCipher not found. Please install with: brew install sqlcipher")
endif()

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
        ${SQLCIPHER_LIBRARY}
        OpenMP::OpenMP_CXX
        utf8cpp::utf8cpp
        zstd::libzstd
)

# Include directories
target_include_directories(magic_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${SQLITE_MODERN_CPP_INCLUDE_DIRS}
    ${SQLCIPHER_INCLUDE_DIR}
)

target_compile_definitions(magic_core PRIVATE
    SQLITE_HAS_CODEC
    SQLCIPHER_CRYPTO_OPENSSL
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