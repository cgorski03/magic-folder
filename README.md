# Magic Folder

An intelligent file management system that uses vector embeddings and semantic search to organize and find your files.

## Overview

Magic Folder is a Rust-based system that automatically processes files, generates embeddings using Ollama, and enables semantic search across your document collection. The project consists of three main components:

- **magic-core**: Core business logic for file processing, vector storage, and embeddings
- **magic-api**: REST API server for file processing and semantic search
- **magic-cli**: Command-line interface for interacting with the system

## Prerequisites

Before testing or running the project, ensure you have:

1. **Rust**: Install the Rust toolchain (edition 2024)

   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

2. **Ollama**: Local Ollama server with the embedding model

   ```bash
   # Install Ollama
   curl -fsSL https://ollama.ai/install.sh | sh

   # Pull the required embedding model
   ollama pull mxbai-embed-large

   # Start Ollama server (if not running)
   ollama serve
   ```

3. **File System**: The project will automatically create required directories:
   - `data/` - Vector and metadata storage
   - `watched_files/` - Directory for test files

## Quick Start

### 1. Build the Project

```bash
git clone <repository-url>
cd magic-folder
cargo build --release
```

### 2. Start the API Server

```bash
cargo run --bin magic-api
```

The server will start on `http://localhost:3030`

### 3. Test with CLI Commands

**Process a file:**

```bash
cargo run --bin magic-cli -- process --file path/to/your/file.txt
```

**Search for files:**

```bash
cargo run --bin magic-cli -- search --query "your search query" --top-k 5
```

## Testing Guide

### Manual Testing Workflow

#### 1. Prepare Test Files

Create sample files to test with:

```bash
mkdir -p watched_files

# Create test documents
echo "This is a comprehensive document about machine learning algorithms and neural networks" > watched_files/ml_doc.txt
echo "A detailed guide to cooking Italian pasta recipes and Mediterranean cuisine" > watched_files/cooking.txt
echo "Financial report for Q4 2024 including revenue analysis and market trends" > watched_files/finance.txt
echo "Software development best practices and coding standards for teams" > watched_files/programming.txt
```

#### 2. Start the API Server

In one terminal:

```bash
cargo run --bin magic-api
```

You should see output indicating the server is running on `127.0.0.1:3030`.

#### 3. Process Files

In another terminal, process each test file:

```bash
# Process test files
cargo run --bin magic-cli -- process --file watched_files/ml_doc.txt
cargo run --bin magic-cli -- process --file watched_files/cooking.txt
cargo run --bin magic-cli -- process --file watched_files/finance.txt
cargo run --bin magic-cli -- process --file watched_files/programming.txt
```

Expected output for successful processing:

```json
{
  "message": "File processed successfully",
  "file_id": 1,
  "vector_id": "watched_files/ml_doc.txt"
}
```

#### 4. Test Search Functionality

Run semantic searches to verify the system works:

```bash
# Search for machine learning content
cargo run --bin magic-cli -- search --query "artificial intelligence" --top-k 3

# Search for cooking content
cargo run --bin magic-cli -- search --query "Italian food recipes" --top-k 3

# Search for financial content
cargo run --bin magic-cli -- search --query "quarterly earnings" --top-k 3

# Search for programming content
cargo run --bin magic-cli -- search --query "software engineering practices" --top-k 3
```

Expected search output:

```json
[
  {
    "path": "watched_files/ml_doc.txt",
    "score": 0.85
  }
]
```

### API Testing with curl

Test the REST API endpoints directly:

**Health check:**

```bash
curl http://localhost:3030/
```

**Process a file:**

```bash
curl -X POST http://localhost:3030/process_file \
  -H "Content-Type: application/json" \
  -d '{"file_path": "watched_files/ml_doc.txt"}'
```

**Search files:**

```bash
curl -X POST http://localhost:3030/search \
  -H "Content-Type: application/json" \
  -d '{"query": "machine learning", "top_k": 5}'
```

### Debugging and Inspection

#### Inspect Vector Store

Use the built-in inspection tool to verify data storage:

```bash
cargo run --bin inspect_vector_store
```

This will display:

- Available tables in the vector database
- Schema information for the 'files' table
- Total number of stored embeddings

#### Enable Debug Logging

For detailed logging during development:

```bash
RUST_LOG=debug cargo run --bin magic-api
```

#### Check Data Directories

Verify that data is being stored correctly:

```bash
# Check metadata database
ls -la data/metadata.sqlite

# Check vector database
ls -la data/vector_data/

# Check test files
ls -la watched_files/
```

## Architecture

### Components

- **Vector Store**: Uses LanceDB for storing high-dimensional embeddings
- **Metadata Store**: SQLite database for file metadata and relationships
- **Ollama Client**: Interfaces with local Ollama server for embedding generation
- **Content Extractor**: Extracts text content from various file formats
- **File Watcher**: (Planned) Automatic monitoring of file system changes

### Data Flow

1. File is submitted for processing via CLI or API
2. Text content is extracted from the file
3. Content is sent to Ollama for embedding generation
4. Embedding is stored in LanceDB vector store
5. File metadata is stored in SQLite database
6. Search queries are embedded and matched against stored vectors

## Testing Checklist

Use this checklist to verify the system is working correctly:

- [ ] Ollama server is running with `mxbai-embed-large` model
- [ ] Project builds without errors (`cargo build`)
- [ ] API server starts successfully on port 3030
- [ ] CLI can process files without errors
- [ ] Vector embeddings are stored (verify with `inspect_vector_store`)
- [ ] Search returns relevant results with similarity scores
- [ ] Metadata is stored in SQLite database (`data/metadata.sqlite`)
- [ ] API endpoints respond correctly to curl requests

## Troubleshooting

### Common Issues

**Ollama Connection Error**

```
Error: Connection refused (os error 61)
```

- Solution: Ensure Ollama is running on port 11434: `ollama serve`

**File Not Found Error**

```
Error: File not found: path/to/file.txt
```

- Solution: Use absolute paths or paths relative to the project root

**Permission Errors**

```
Error: Permission denied
```

- Solution: Ensure the `data/` directory is writable by the current user

**Empty Search Results**

```
Search results: []
```

- Solution: Make sure files have been processed first using the `process` command

**Model Not Found**

```
Error: Model 'mxbai-embed-large' not found
```

- Solution: Pull the model with `ollama pull mxbai-embed-large`

### Debug Steps

1. **Check Ollama Status:**

   ```bash
   curl http://localhost:11434/api/tags
   ```

2. **Verify File Processing:**

   ```bash
   cargo run --bin inspect_vector_store
   ```

3. **Check Logs:**
   ```bash
   RUST_LOG=debug cargo run --bin magic-api
   ```

## Development

### Adding Tests

Currently, the project lacks formal unit tests. To contribute tests:

1. Add test dependencies to `Cargo.toml`:

   ```toml
   [dev-dependencies]
   tokio-test = "0.4"
   tempfile = "3.0"
   ```

2. Create test modules:

   ```rust
   #[cfg(test)]
   mod tests {
       use super::*;

       #[tokio::test]
       async fn test_file_processing() {
           // Test implementation
       }
   }
   ```

### Running Tests

```bash
cargo test
```

## Future Enhancements

- [ ] Automatic file watching and processing
- [ ] Support for more file formats
- [ ] Web UI for file management
- [ ] Integration tests
- [ ] Performance benchmarks
- [ ] Docker containerization
- [ ] Configuration file support

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## License

[Add your license information here]
