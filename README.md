# Magic Folder C++

An intelligent file management system leveraging locally-run LLMs and embedding models to organize and classify your files with an unyielding commitment to privacy and performance.

## 🎯 Project Status: MVP with Roadmap

**Current State**: Basic file indexing and semantic search working with single-file processing
**Next Phase**: Advanced chunking system with worker pools for scalable processing

## 🏗️ Architecture Overview

Magic Folder C++ is a C++ implementation that automatically processes files, generates embeddings using Ollama, and enables semantic search across your document collection. The system is designed with a layered architecture:

### Core Components

- **`magic-core`**: Core business logic for file processing, vector storage, and embeddings
- **`magic-api`**: REST API server with background worker pools
- **`magic-cli`**: Command-line interface for system interaction

### Current MVP Features ✅

- ✅ Basic file content extraction (text, markdown, code files)
- ✅ Content hashing for change detection
- ✅ Embedding generation via Ollama (`mxbai-embed-large`)
- ✅ SQLite metadata storage with FAISS vector search
- ✅ REST API endpoints for file processing and search
- ✅ CLI interface for system interaction
- ✅ Comprehensive test coverage

### Next Phase Features 🚧

- 🚧 **Advanced Chunking System**: Multi-level content chunking with semantic boundaries
- 🚧 **Worker Pool Architecture**: Asynchronous background processing
- 🚧 **Task Queue System**: Reliable job processing with retry logic
- 🚧 **Enhanced Search**: Two-stage search (file-level + chunk-level)
- 🚧 **File Watching**: Real-time file system monitoring

## 📋 Development Roadmap

### Phase 1: Chunking System Overhaul (Priority 1)

#### 1.1 Content Extractor Refactoring
- [ ] **Create `Chunk.h` structure**
  - [ ] Define `Chunk` struct with content, metadata, and vector fields
  - [ ] Add chunk indexing and ordering logic
  - [ ] Implement chunk-level content hashing

- [ ] **Implement `ContentExtractorFactory`**
  - [ ] Create factory pattern for file type detection
  - [ ] Add support for multiple extractor types
  - [ ] Implement fallback to `PlainTextExtractor`

- [ ] **Build `MarkdownExtractor`**
  - [ ] Parse markdown structure (headers, sections, lists)
  - [ ] Create semantic chunk boundaries
  - [ ] Preserve markdown formatting in chunks
  - [ ] Add metadata extraction (title, headings, links)

- [ ] **Enhance `PlainTextExtractor`**
  - [ ] Implement paragraph-based chunking
  - [ ] Add sentence boundary detection
  - [ ] Handle special characters and encoding
  - [ ] Add content cleaning and normalization

#### 1.2 Database Schema Updates
- [ ] **Update `Files` table**
  - [ ] Add `summary_vector_blob` for file-level embeddings
  - [ ] Add `processing_status` field
  - [ ] Add `suggested_category` and `suggested_filename` fields
  - [ ] Add `tags` JSON field

- [ ] **Create `Chunks` table**
  - [ ] Implement chunk storage with file relationships
  - [ ] Add chunk indexing and ordering
  - [ ] Store chunk-level vectors
  - [ ] Add content hashing for chunks

- [ ] **Create `TaskQueue` table**
  - [ ] Implement task status tracking
  - [ ] Add priority and error handling
  - [ ] Support task retry logic

#### 1.3 Search System Enhancement
- [ ] **Two-Stage Search Implementation**
  - [ ] Stage 1: File-level similarity search
  - [ ] Stage 2: Chunk-level precision search
  - [ ] Implement result ranking and scoring
  - [ ] Add search result highlighting

- [ ] **Memory Index Management**
  - [ ] Load summary vectors at startup
  - [ ] Implement chunk index loading
  - [ ] Add index rebuilding capabilities
  - [ ] Optimize memory usage

### Phase 2: Worker Pool Architecture (Priority 2)

#### 2.1 Async Infrastructure
- [ ] **Create `Worker.h` interface**
  - [ ] Define worker thread lifecycle
  - [ ] Add task processing interface
  - [ ] Implement error handling and recovery
  - [ ] Add worker health monitoring

- [ ] **Implement `WorkerPool.h`**
  - [ ] Create thread pool management
  - [ ] Add load balancing logic
  - [ ] Implement graceful shutdown
  - [ ] Add worker pool configuration

- [ ] **Database Integration**
  - [ ] Create `DatabaseManager.h` for connection pooling
  - [ ] Implement `TaskQueue.h` for job management
  - [ ] Add transaction handling
  - [ ] Implement connection retry logic

#### 2.2 Background Processing
- [ ] **Task Processing Pipeline**
  - [ ] Implement task polling mechanism
  - [ ] Add task status updates
  - [ ] Create error handling and retry logic
  - [ ] Add progress tracking

- [ ] **File System Integration**
  - [ ] Implement file watching capabilities
  - [ ] Add file change detection
  - [ ] Create automatic task creation
  - [ ] Add file move/rename handling

### Phase 3: Enhanced Features (Priority 3)

#### 3.1 Advanced Content Processing
- [ ] **PDF Support**
  - [ ] Integrate PDF text extraction library
  - [ ] Handle PDF structure and formatting
  - [ ] Add PDF metadata extraction
  - [ ] Implement PDF chunking strategies

- [ ] **Code File Enhancement**
  - [ ] Add syntax-aware chunking
  - [ ] Implement function/class boundary detection
  - [ ] Add code comment extraction
  - [ ] Create code-specific search features

#### 3.2 Search and UI Improvements
- [ ] **Advanced Search Features**
  - [ ] Add search filters (file type, date, size)
  - [ ] Implement search result pagination
  - [ ] Add search history and suggestions
  - [ ] Create search result export

- [ ] **Performance Optimizations**
  - [ ] Implement vector caching
  - [ ] Add search result caching
  - [ ] Optimize database queries
  - [ ] Add background index maintenance

## 🛠️ Installation & Setup

### Prerequisites

1. **C++20 Compatible Compiler**: GCC 10+, Clang 12+, or MSVC 2019+
2. **CMake**: Version 3.20 or higher
3. **Ollama**: Local Ollama server with embedding model

```bash
# Install Ollama
curl -fsSL https://ollama.ai/install.sh | sh

# Pull the required embedding model
ollama pull mxbai-embed-large

# Start Ollama server
ollama serve
```

4. **Dependencies**:
   - libcurl (HTTP client)
   - nlohmann-json (JSON parsing)
   - sqlite3 (Database)
   - OpenSSL (Cryptographic functions)
   - FAISS (Vector similarity search)

### Installation

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev libsqlite3-dev libssl-dev
```

#### macOS
```bash
# Using Homebrew
brew install cmake curl nlohmann-json sqlite openssl

# Using MacPorts
sudo port install cmake curl nlohmann-json sqlite3 openssl
```

#### Windows
```bash
# Using vcpkg
vcpkg install curl nlohmann-json sqlite3 openssl faiss
```

### Building

```bash
# Clone and navigate
cd magic-folder-cpp

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)  # Linux/macOS
# or
cmake --build . --config Release  # Windows
```

## ⚙️ Configuration

Create a `.env` file in the project root:

```bash
# API Configuration
API_BASE_URL=127.0.0.1:3030

# Database Paths
VECTOR_DB_PATH=./data/vector_db
METADATA_DB_PATH=./data/metadata.db

# Ollama Configuration
OLLAMA_URL=http://localhost:11434
EMBEDDING_MODEL=mxbai-embed-large

# Worker Pool Configuration
WORKER_POOL_SIZE=4
TASK_QUEUE_POLL_INTERVAL=1000

# File Watching
WATCHED_FOLDER=./watched_files
```

## 🚀 Usage

### 1. Start the API Server

```bash
# From the build directory
./bin/magic_api
```

The server starts on `http://localhost:3030` with background worker pools.

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

**Check system status**:
```bash
./bin/magic_cli status
```

### 3. API Endpoints

- `GET /` - Health check
- `POST /process_file` - Process a file for indexing
- `POST /search` - Search for files using semantic search
- `GET /files` - List all indexed files
- `GET /files/{path}` - Get information about a specific file
- `DELETE /files/{path}` - Delete a file from the index
- `GET /status` - Get system status and worker pool info

## 📁 Project Structure

### Current Structure (MVP)

```
magic-folder-cpp/
├── .env                      # Configuration (DB paths, Ollama URL, worker count)
├── .gitignore                # Files to ignore for version control
├── CMakeLists.txt            # Root CMake file
├── LICENSE                   # Project license
├── README.md                 # This file
├── SystemDesignDiagrams.md   # System architecture diagrams
├── TESTING.md                # Testing documentation
├── vcpkg.json                # Package dependencies
├── build.sh                  # Build script
├── run_tests.sh              # Test runner script
│
├── data/                     # Runtime data (gitignored)
│   └── metadata.db           # SQLite database
│
├── include/                  # PUBLIC HEADERS
│   ├── magic_core/           # Core library headers
│   │   ├── content_extractor.hpp    # Content extraction interface
│   │   ├── metadata_store.hpp       # Database and vector storage
│   │   ├── ollama_client.hpp        # Ollama API client
│   │   ├── types.hpp                # Core type definitions
│   │   └── file_watcher.hpp         # File system monitoring
│   │
│   ├── magic_api/            # API server headers
│   │   ├── config.hpp        # Server configuration
│   │   ├── routes.hpp        # HTTP route handlers
│   │   └── server.hpp        # Server interface
│   │
│   ├── magic_services/       # Service layer headers
│   │   ├── file_processing_service.hpp  # File processing orchestration
│   │   ├── search_service.hpp           # Semantic search service
│   │   ├── file_info_service.hpp       # File metadata service
│   │   ├── file_delete_service.hpp     # File deletion service
│   │   └── background/                 # Background processing
│   │
│   └── magic_cli/            # CLI headers
│       └── cli_handler.hpp   # Command-line interface
│
├── src/                      # IMPLEMENTATION
│   ├── magic_core/           # Core library implementation
│   │   ├── content_extractor.cpp      # Content extraction logic
│   │   ├── metadata_store.cpp         # Database operations
│   │   ├── ollama_client.cpp          # Ollama API implementation
│   │   ├── types.cpp                  # Type implementations
│   │   ├── file_watcher.cpp           # File system monitoring
│   │   └── CMakeLists.txt
│   │
│   ├── magic_api/            # API server executable
│   │   ├── main.cpp          # Server entry point
│   │   ├── server.cpp        # Server implementation
│   │   ├── routes.cpp        # HTTP endpoint handlers
│   │   └── CMakeLists.txt
│   │
│   ├── magic_services/       # Service layer implementation
│   │   ├── file_processing_service.cpp # File processing logic
│   │   ├── search_service.cpp         # Search implementation
│   │   ├── file_info_service.cpp      # File metadata operations
│   │   ├── file_delete_service.cpp    # File deletion logic
│   │   └── CMakeLists.txt
│   │
│   └── magic_cli/            # CLI executable
│       ├── main.cpp          # CLI entry point
│       ├── cli_handler.cpp   # CLI command handling
│       └── CMakeLists.txt
│
├── tests/                    # Unit and integration tests
│   ├── test_file_processing_service.cpp # File processing tests
│   ├── test_search_service.cpp         # Search functionality tests
│   ├── test_file_delete_service.cpp    # File deletion tests
│   ├── test_file_info_service.cpp      # File info tests
│   ├── test_utilities.cpp              # Utility function tests
│   ├── test_mocks.hpp                  # Mock objects for testing
│   ├── test_utilities.hpp              # Test utilities
│   ├── test_main.cpp                   # Test entry point
│   └── CMakeLists.txt
│
├── third_party/              # Third-party dependencies
└── build/                    # Build output directory
```

### Future Structure (Phase 1+)

The core will be enhanced with the following structure:

```
include/magic_core/
├── async/
│   ├── Worker.h              # Worker thread interface
│   └── WorkerPool.h          # Worker pool management
├── db/
│   ├── DatabaseManager.h     # SQLite connection management
│   └── TaskQueue.h           # Task queue interface
├── extractors/
│   ├── ContentExtractor.h    # Abstract base class
│   ├── ContentExtractorFactory.h # Factory pattern
│   ├── MarkdownExtractor.h   # Markdown-specific extractor
│   └── PlainTextExtractor.h  # Fallback extractor
├── llm/
│   └── OllamaClient.h        # Ollama API client
└── Chunk.h                   # Chunk data structure
```

This future structure will be implemented during Phase 1 of the development roadmap.

## 🔧 Development Status

### ✅ Completed (MVP)
- Basic project structure and CMake configuration
- Core component interfaces and implementations
- CLI argument parsing and API communication
- SQLite metadata storage with FAISS integration
- Content extraction for text files
- HTTP client implementation using libcurl
- Comprehensive test coverage

### 🚧 In Progress (Phase 1)
- Content extractor refactoring with chunking system
- Database schema updates for chunks and task queue
- Two-stage search implementation
- Memory index management

### ❌ Not Yet Implemented
- Worker pool architecture (Phase 2)
- Advanced file watching (Phase 2)
- PDF content extraction (Phase 3)
- Async operations (Phase 2)
- Performance optimizations (Phase 3)

## 🧪 Testing

Run the test suite:

```bash
# From build directory
./bin/run_tests

# Or run specific tests
./bin/test_chunking
./bin/test_db
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License

## 🔮 Future Enhancements

### Phase 4: Advanced Features
- [ ] Docker containerization
- [ ] CI/CD pipeline
- [ ] Performance monitoring
- [ ] Advanced analytics
- [ ] Plugin system for custom extractors

### Phase 5: Enterprise Features
- [ ] Multi-user support
- [ ] Access control and permissions
- [ ] Audit logging
- [ ] Backup and restore
- [ ] Cluster deployment

## 🐛 Troubleshooting

### Common Issues

1. **Ollama not running**: Start with `ollama serve`
2. **Missing dependencies**: Install all required libraries
3. **Build errors**: Ensure C++20 compiler and CMake 3.20+
4. **Permission errors**: Check write permissions for data directory
5. **Port conflicts**: Change API_BASE_URL in .env file

### Debug Mode

Enable debug logging by setting environment variables:
```bash
export MAGIC_FOLDER_DEBUG=1
export MAGIC_FOLDER_LOG_LEVEL=DEBUG
```

## 📊 Performance Notes

- **Current**: Single-threaded processing, ~1-2 files/second
- **Target (Phase 2)**: Multi-threaded with worker pools, ~10-20 files/second
- **Memory Usage**: ~100MB base + ~1MB per 1000 files
- **Storage**: ~1KB per file metadata + ~4KB per chunk vector

---

**Next Steps**: Start with Phase 1 - Content Extractor Refactoring to implement the chunking system foundation.
