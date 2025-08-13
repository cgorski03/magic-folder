# Magic Folder C++

An intelligent file management system leveraging locally-run LLMs and embedding models to organize and classify your files with an unyielding commitment to privacy and performance.

## Project Status: MVP with Roadmap

**Current State**: Chunked file indexing (Markdown + Plaintext), zstd-compressed chunk storage, SQLCipher-encrypted SQLite, FAISS-backed two-stage search (files + chunks), background Worker + WorkerPool processing via task queue, REST API + CLI working against a live Ollama server
**Next Phase**: File watching, worker health/metrics, background index maintenance, PDF extractor

## Architecture Overview

Magic Folder C++ is a C++ implementation that automatically processes files, generates embeddings using Ollama, and enables semantic search across your document collection. The system is designed with a layered architecture:

### Core Components

- **`magic-core`**: Core business logic for file processing, vector storage, and embeddings
- **`magic-api`**: REST API server with background worker pools
- **`magic-cli`**: Command-line interface for system interaction

### Current MVP Features 

- Chunked content extraction for Markdown and Plaintext
- Content hashing for change detection (single read: hash + chunks)
- Embedding generation via Ollama (`mxbai-embed-large`)
- SQLCipher-encrypted SQLite metadata store (macOS keychain-backed key)
- zstd compression of chunk content at rest
- FAISS in-memory index for file-level search; on-demand FAISS for chunk search
- Background processing pipeline: task queue + worker threads (`Worker`, `WorkerPool`)
- REST API endpoints for processing and search (file-only and combined)
- CLI for process/search/list
- Comprehensive unit tests with mocks

### Next Phase Features 

- **Advanced Chunking System**: Additional extractors (e.g., code-aware, PDFs)
- **Worker Health + Metrics**: Monitoring, backoff, instrumentation
- **Index Maintenance**: Background rebuild/compaction
- **File Watching**: Real-time file system monitoring and auto-enqueue

## Development Roadmap

### Phase 1: Chunking System Overhaul (Priority 1)

#### 1.1 Content Extractor Refactoring
- [x] **Create `Chunk.h` structure**
  - [x] Define `Chunk` struct with content, metadata, and vector fields
  - [x] Add chunk indexing and ordering logic
  - [x] Implement chunk-level content hashing

- [x] **Implement `ContentExtractorFactory`**
  - [x] Create factory pattern for file type detection
  - [x] Add support for multiple extractor types
  - [x] Implement fallback to `PlainTextExtractor`

- [x] **Build `MarkdownExtractor`**
  - [x] Parse markdown structure (headers, sections, lists)
  - [x] Create semantic chunk boundaries
  - [x] Preserve markdown formatting in chunks
  - [x] Add metadata extraction (title, headings, links)

- [x] **Enhance `PlainTextExtractor`**
  - [x] Implement paragraph-based chunking
  - [x] Add sentence boundary detection
  - [x] Handle special characters and encoding
  - [x] Add content cleaning and normalization

#### 1.2 Database Schema Updates
- [x] **Update `Files` table**
  - [x] Add `summary_vector_blob` for file-level embeddings
  - [x] Add `processing_status` field
  - [x] Add `suggested_category` and `suggested_filename` fields
  - [x] Add `tags` JSON field

- [x] **Create `Chunks` table**
  - [x] Implement chunk storage with file relationships
  - [x] Add chunk indexing and ordering
  - [x] Store chunk-level vectors
  - [x] Add content hashing for chunks

- [x] **Create `TaskQueue` table**
  - [x] Implement task status tracking
  - [x] Add priority and error handling
  - [x] Support task retry logic
#### 1.25 Data Storage Optimization
- [x] **Encode `content` blob**
  - [x] Setup zstd library integration
  - [x] Implement encoding in the content blob field for the chunks

- [x] **Encrypt Data at rest**
  - [x] Implement OS-backed key management (macOS Keychain)
  - [x] Migrate database to `sqlite-modern-cpp`
  - [x] Migrate database to SQLCipher

#### 1.3 Search System Enhancement
- [x] **Two-Stage Search Implementation**
  - [x] Stage 1: File-level similarity search
  - [x] Stage 2: Chunk-level precision search
  - [x] Implement result ranking and scoring
  - [x] Add search result highlighting

- [x] **Memory Index Management**
  - [x] Load summary vectors at startup
  - [x] Implement chunk index loading
  - [x] Add index rebuilding capabilities
  - [x] Optimize memory usage

### Phase 2: Worker Pool Architecture (Priority 2)

#### 2.1 Async Infrastructure
- [x] **Create `Worker.h` interface**
  - [x] Define worker thread lifecycle
  - [x] Add task processing interface
  - [x] Implement error handling and recovery
  - [ ] Add worker health monitoring

- [x] **Implement `WorkerPool.h`**
  - [x] Thread pool management
  - [x] Graceful shutdown coordination
  - [x] Configurable worker count (via `magicrc.json`)

- [x] **Database Integration**
  - [x] Create `DatabaseManager.h` for connection pooling
  - [x] Implement `TaskQueue.h` for job management
  - [x] Add transaction handling
  - [ ] Implement connection retry logic

#### 2.2 Background Processing
- [x] **Task Processing Pipeline**
  - [x] Implement task polling mechanism
  - [x] Add task status updates
  - [x] Create error handling and retry logic
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
   - curl (HTTP client)
   - nlohmann-json (JSON parsing)
   - sqlite (builds against SQLCipher)
   - SQLCipher (encrypted SQLite)
   - FAISS (vector similarity search)
   - zstd (chunk compression)
   - crow (HTTP server)

### Installation

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev libsqlcipher-dev libfaiss-dev libzstd-dev
```

#### macOS
```bash
# Using Homebrew
brew install cmake curl nlohmann-json sqlcipher faiss zstd

# Optional: vcpkg for C++ dependencies
# https://learn.microsoft.com/vcpkg/get_started/get-started
```

#### Windows
```bash
# Using vcpkg
vcpkg install curl nlohmann-json sqlite3 faiss zstd crow
```

### Building

```bash
# From repo root
mkdir build && cd build
cmake ..
cmake --build . -j
```

## ⚙️ Configuration

Create a `magicrc.json` in the project root (read at server startup):

```json
{
  "api_base_url": "127.0.0.1:3030",
  "metadata_db_path": "./data/metadata.db",
  "ollama_url": "http://localhost:11434",
  "embedding_model": "mxbai-embed-large",
  "num_workers": 4
}
```

Notes:
- The server uses SQLCipher with a key fetched from the OS keychain (macOS only). On non-macOS platforms the server will currently throw when requesting the key.
- The CLI reads `API_BASE_URL` from the environment (default: `http://127.0.0.1:3030`).

## Usage

### 1. Start the API Server

```bash
# From the build directory
./bin/magic_api
```

The server starts on `http://localhost:3030` (or your configured host:port) and launches a `WorkerPool` of `num_workers` threads to process tasks from the queue.

### 2. Use the CLI

Environment (optional):
```bash
export API_BASE_URL=http://127.0.0.1:3030
```

Commands:
- Process a file:
  ```bash
  ./bin/magic_cli process --file /path/to/file.txt
  ```
- Magic search (files + chunks):
  ```bash
  ./bin/magic_cli search --query "your query" --top-k 5
  ```
- File-only search:
  ```bash
  ./bin/magic_cli filesearch --query "your query" --top-k 5
  ```
- List all files:
  ```bash
  ./bin/magic_cli list
  ```

### 3. API Endpoints

- `GET /` - Health check
- `POST /process_file` - Queue a file for processing
- `POST /search` - Magic search: returns top-k files and top-k chunks (with decompressed content)
- `POST /files/search` - File-only search
- `GET /files` - List all indexed files
- `GET /files/{path}` - Get file info (placeholder response for now)
- `DELETE /files/{path}` - Delete file (placeholder)

## 📁 Project Structure

```
magic-folder/
├── README.md
├── CMakeLists.txt
├── vcpkg.json
├── build.sh
├── run_tests.sh
├── magicrc.json                 # JSON config (server)
├── data/                        # Runtime data (gitignored)
│   └── metadata.db
├── include/
│   ├── magic_api/
│   │   ├── config.hpp
│   │   ├── routes.hpp
│   │   └── server.hpp
│   ├── magic_cli/
│   │   └── cli_handler.hpp
│   └── magic_core/
│       ├── async/               # Worker + pool
│       ├── db/                  # SQLCipher + schema + repos
│       ├── extractors/          # Markdown, Plaintext, Factory
│       ├── llm/                 # Ollama client
│       ├── services/            # File/Search/Info/Delete
│       └── types/               # Chunk, File
├── src/
│   ├── magic_api/               # main.cpp, routes.cpp, server.cpp
│   ├── magic_cli/               # main.cpp, cli_handler.cpp
│   └── magic_core/              # implementations for include/
└── tests/
    ├── unit/
    ├── common/
    └── test_main.cpp
```

## 🧪 Testing

See detailed instructions in TESTING.md. Run the test suite:

```bash
# From repo root
export VCPKG_ROOT=/path/to/vcpkg  # required by run_tests.sh
./run_tests.sh
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
2. **Missing FAISS/SQLCipher**: Install dev packages (see prerequisites)
3. **Build errors**: Ensure C++20 compiler and CMake 3.20+
4. **Permission errors**: Check write permissions for `data/`
5. **Port conflicts**: Change `api_base_url` in `magicrc.json`
6. **Non-macOS server startup**: Server requires macOS Keychain for DB key; non-macOS not yet supported for server runtime

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