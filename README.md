# Magic Folder C++

An intelligent file management system that uses vector embeddings and semantic search to organize and find your files, implemented in C++.

## Overview

Magic Folder C++ is a C++ implementation of the Magic Folder system that automatically processes files, generates embeddings using Ollama, and enables semantic search across your document collection. The project consists of three main components:

- **magic-core**: Core business logic for file processing, vector storage, and embeddings
- **magic-api**: REST API server for file processing and semantic search
- **magic-cli**: Command-line interface for interacting with the system

## Prerequisites

Before building or running the project, ensure you have:

1. **C++20 Compatible Compiler**: GCC 10+, Clang 12+, or MSVC 2019+
2. **CMake**: Version 3.20 or higher
3. **Ollama**: Local Ollama server with the embedding model

   ```bash
   # Install Ollama
   curl -fsSL https://ollama.ai/install.sh | sh

   # Pull the required embedding model
   ollama pull mxbai-embed-large

   # Start Ollama server (if not running)
   ollama serve
   ```

4. **Dependencies**: The following libraries need to be installed:
   - libcurl (HTTP client)
   - nlohmann-json (JSON parsing)
   - sqlite3 (Database)
   - OpenSSL (Cryptographic functions)

## Installation

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev libsqlite3-dev libssl-dev
```

### macOS

```bash
# Using Homebrew
brew install cmake curl nlohmann-json sqlite openssl

# Using MacPorts
sudo port install cmake curl nlohmann-json sqlite3 openssl
```

### Windows

```bash
# Using vcpkg
vcpkg install curl nlohmann-json sqlite3 openssl
```

## Building the Project

1. **Clone and navigate to the project**:

   ```bash
   cd magic-folder-cpp
   ```

2. **Create build directory**:

   ```bash
   mkdir build && cd build
   ```

3. **Configure with CMake**:

   ```bash
   cmake ..
   ```

4. **Build the project**:

   ```bash
   make -j$(nproc)  # Linux/macOS
   # or
   cmake --build . --config Release  # Windows
   ```

5. **Install (optional)**:
   ```bash
   sudo make install
   ```

## Configuration

Create a `.env` file in the project root with the following variables:

```bash
# API Configuration
API_BASE_URL=127.0.0.1:3030

# Database Paths
VECTOR_DB_PATH=./data/vector_db
METADATA_DB_PATH=./data/metadata.db

# Ollama Configuration
OLLAMA_URL=http://localhost:11434
EMBEDDING_MODEL=mxbai-embed-large

# File Watching
WATCHED_FOLDER=./watched_files
```

## Usage

### 1. Start the API Server

```bash
# From the build directory
./bin/magic_api
```

The server will start on `http://localhost:3030`

### 2. Use the CLI

**Process a file**:

```bash
./bin/magic_cli process --file path/to/your/file.txt
```

**Search for files**:

```bash
./bin/magic_cli search --query "your search query" --top-k 5
```

**List all files**:

```bash
./bin/magic_cli list
```

**Show help**:

```bash
./bin/magic_cli help
```

### 3. API Endpoints

The API server provides the following REST endpoints:

- `GET /` - Health check
- `POST /process_file` - Process a file for indexing
- `POST /search` - Search for files using semantic search
- `GET /files` - List all indexed files
- `GET /files/{path}` - Get information about a specific file
- `DELETE /files/{path}` - Delete a file from the index

## Project Structure

```
magic-folder-cpp/
├── CMakeLists.txt              # Main CMake configuration
├── include/                    # Header files
│   ├── magic_core/            # Core library headers
│   ├── magic_api/             # API server headers
│   └── magic_cli/             # CLI headers
├── src/                       # Source files
│   ├── magic_core/            # Core library implementation
│   ├── magic_api/             # API server implementation
│   └── magic_cli/             # CLI implementation
├── tests/                     # Test files
├── third_party/               # Third-party dependencies
└── build/                     # Build output directory
```

## Key Components

### magic-core

- **OllamaClient**: Handles HTTP requests to Ollama API for embeddings
- **VectorStore**: Manages vector database operations (placeholder for LanceDB/FAISS)
- **MetadataStore**: SQLite-based metadata storage for files
- **ContentExtractor**: Extracts and processes content from various file types
- **FileWatcher**: Monitors directories for file changes (platform-specific)

### magic-api

- **Server**: HTTP server implementation (placeholder)
- **Routes**: REST API endpoint handlers

### magic-cli

- **CliHandler**: Command-line argument parsing and API communication

## Development Status

This is a work-in-progress C++ migration of the original Rust Magic Folder project. The current implementation includes:

✅ **Completed**:

- Basic project structure and CMake configuration
- Core component interfaces and basic implementations
- CLI argument parsing and API communication
- SQLite metadata storage
- Content extraction for text files
- HTTP client implementation using libcurl

🔄 **In Progress**:

- Vector database integration (LanceDB/FAISS)
- HTTP server implementation
- File watching (platform-specific)
- PDF content extraction

❌ **Not Yet Implemented**:

- Advanced vector similarity search
- Async operations
- Comprehensive error handling
- Unit tests
- Performance optimizations

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Migration Notes

This C++ implementation is a migration from the original Rust project. Key differences:

- **Memory Management**: Uses RAII and smart pointers instead of Rust's ownership system
- **Error Handling**: Uses exceptions instead of Result types
- **Async Programming**: Will use std::async or third-party libraries instead of Tokio
- **Package Management**: Uses CMake and system package managers instead of Cargo

## Troubleshooting

### Common Build Issues

1. **Missing dependencies**: Ensure all required libraries are installed
2. **CMake version**: Update to CMake 3.20 or higher
3. **Compiler version**: Use a C++20 compatible compiler

### Runtime Issues

1. **Ollama not running**: Start Ollama server with `ollama serve`
2. **Port conflicts**: Change API_BASE_URL in .env file
3. **Permission errors**: Ensure write permissions for data directories

## Future Enhancements

- [ ] Implement actual vector database (LanceDB C++ SDK or FAISS)
- [ ] Add comprehensive HTTP server (cpp-httplib or Crow)
- [ ] Implement platform-specific file watching
- [ ] Add PDF content extraction
- [ ] Implement async operations
- [ ] Add comprehensive test suite
- [ ] Performance optimizations
- [ ] Docker containerization
- [ ] CI/CD pipeline
