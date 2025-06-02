use axum::Router;
use magic_api::{
    AppState, ProcessFileRequest, ProcessFileResponse, SearchRequest, SearchResponseFile,
    create_app,
};
use magic_core::{
    metadata_store::MetadataStore, ollama_client::OllamaClient, vector_store::VectorStore,
};
use reqwest::Client as TestClient;
use std::env;
use std::fs::{File, create_dir_all};
use std::io::Write;
use std::sync::Arc;
use tempfile::tempdir;
use tokio::net::TcpListener;

// --- Configuration for Live Ollama Tests ---
// Ensure these are set in your environment or use defaults.
// And ensure the model is pulled in your Ollama instance.
const DEFAULT_LIVE_OLLAMA_URL: &str = "http://localhost:11434";
const LIVE_OLLAMA_EMBEDDING_MODEL: &str = "mxbai-embed-large";

// Helper to spawn the app with a specific test state, configured for live Ollama
async fn spawn_app_for_live_ollama_test() -> String {
    let listener = TcpListener::bind("127.0.0.1:0")
        .await
        .expect("Failed to bind random port");
    let app_address = format!("http://{}", listener.local_addr().unwrap());

    let temp_data_dir = tempdir().expect("Failed to create temp dir for test data");
    let metadata_db_path = temp_data_dir.path().join("live_api_meta.sqlite");
    let vector_db_path_dir = temp_data_dir.path().join("live_api_vector_data");
    create_dir_all(&vector_db_path_dir)
        .expect("Failed to create test vector DB dir for app instance");

    let ollama_url = env::var("INTELLIFOLDER_TEST_OLLAMA_URL")
        .unwrap_or_else(|_| DEFAULT_LIVE_OLLAMA_URL.to_string());
    let ollama_model = env::var("INTELLIFOLDER_TEST_OLLAMA_EMBED_MODEL")
        .unwrap_or_else(|_| LIVE_OLLAMA_EMBEDDING_MODEL.to_string());

    tracing::info!(
        "Integration Test: Connecting to Ollama at: {} with model: {}",
        ollama_url,
        ollama_model
    );

    // Pre-flight check: Try to connect to Ollama or list models
    // This helps fail fast if Ollama isn't ready.
    let ollama_check_client = reqwest::Client::new();
    match ollama_check_client
        .get(format!("{}/api/tags", ollama_url))
        .send()
        .await
    {
        Ok(resp) if resp.status().is_success() => {
            tracing::info!(
                "Integration Test: Successfully connected to Ollama for pre-flight check."
            );
            // Optionally, check if the specific model exists in the response
        }
        Ok(resp) => {
            panic!(
                "Integration Test: Ollama pre-flight check failed! Status: {}. Body: {:?}. Ensure Ollama is running at {} and accessible.",
                resp.status(),
                resp.text().await,
                ollama_url
            );
        }
        Err(e) => {
            panic!(
                "Integration Test: Ollama pre-flight check failed! Error: {}. Ensure Ollama is running at {} and accessible.",
                e, ollama_url
            );
        }
    }

    let ollama_client = Arc::new(OllamaClient::new(ollama_url, ollama_model));
    let metadata_store = Arc::new(
        MetadataStore::new(&metadata_db_path)
            .await
            .expect("Failed to init test metadata store for app instance"),
    );
    let vector_store = Arc::new(
        VectorStore::new(&vector_db_path_dir, "live_test_table")
            .await
            .expect("Failed to init test vector store for app instance"),
    );

    let test_app_state = AppState {
        ollama_client,
        metadata_store,
        vector_store,
    };

    let app: Router = create_app(Some(test_app_state)).await;

    tokio::spawn(async move {
        axum::serve(listener, app.into_make_service())
            .await
            .unwrap();
        let _keep_temp_dir = temp_data_dir; // Keep alive
    });

    app_address
}

#[tokio::test]
#[ignore = "Integration test: Requires a running Ollama instance with the specified model pulled."]
async fn test_live_ollama_process_file_endpoint_success() {
    let app_address = spawn_app_for_live_ollama_test().await;
    let client = TestClient::new();

    let temp_file_dir = tempdir().unwrap();
    let file_to_process = temp_file_dir.path().join("live_process_test.txt");
    let mut file = File::create(&file_to_process).unwrap();
    let file_content =
        "This is some unique text content for live Ollama processing during integration tests.";
    writeln!(file, "{}", file_content).unwrap();
    let file_path_str = file_to_process.to_str().unwrap().to_string();

    tracing::info!("TEST (Live Ollama): Processing file: {}", file_path_str);
    let request_payload = ProcessFileRequest {
        file_path: file_path_str.clone(),
    };
    let response = client
        .post(format!("{}/process_file", app_address))
        .json(&request_payload)
        .send()
        .await
        .expect("Failed to call /process_file");

    let status = response.status();
    let body_text = response.text().await.unwrap_or_default();
    assert!(
        status.is_success(),
        "Process file failed with status {}: {}",
        status,
        body_text
    );

    let response_body: ProcessFileResponse = serde_json::from_str(&body_text).unwrap_or_else(|e| {
        panic!(
            "Failed to parse /process_file response: {}. Body: {}",
            e, body_text
        )
    });
    assert_eq!(response_body.message, "File processed successfully");
    assert!(response_body.file_id.is_some());
    assert_eq!(response_body.vector_id, Some(file_path_str));
    tracing::info!("TEST (Live Ollama): File processed successfully via API.");
}

#[tokio::test]
#[ignore = "Integration test: Requires a running Ollama instance with the specified model pulled."]
async fn test_live_ollama_search_endpoint_finds_processed_file() {
    let app_address = spawn_app_for_live_ollama_test().await;
    let client = TestClient::new();

    // 1. Pre-process a file
    let temp_file_dir = tempdir().unwrap();
    let file_to_process = temp_file_dir.path().join("live_search_target.md"); // Use a different extension
    let mut file = File::create(&file_to_process).unwrap();
    let file_content = "An interesting document about advanced AI techniques and vector databases for semantic retrieval using Rust.";
    writeln!(file, "{}", file_content).unwrap();
    let file_path_str = file_to_process.to_str().unwrap().to_string();

    tracing::info!(
        "TEST (Live Ollama): Pre-processing file for search: {}",
        file_path_str
    );
    let process_payload = ProcessFileRequest {
        file_path: file_path_str.clone(),
    };
    let process_response = client
        .post(format!("{}/process_file", app_address))
        .json(&process_payload)
        .send()
        .await
        .expect("Failed to process file for search test");
    assert!(
        process_response.status().is_success(),
        "Pre-search file processing failed."
    );

    // Allow time for backend processing and DB writes to complete.
    // This might need adjustment based on your system and Ollama speed.
    tokio::time::sleep(tokio::time::Duration::from_secs(2)).await;

    // 2. Search for content related to the processed file
    let search_query_text = "semantic vector retrieval in Rust AI";
    tracing::info!(
        "TEST (Live Ollama): Searching for query: '{}'",
        search_query_text
    );
    let search_payload = SearchRequest {
        query: search_query_text.to_string(),
        top_k: Some(3),
    };
    let search_response = client
        .post(format!("{}/search", app_address))
        .json(&search_payload)
        .send()
        .await
        .expect("Failed to call /search");

    let status = search_response.status();
    let body_text = search_response.text().await.unwrap_or_default();
    assert!(
        status.is_success(),
        "Search endpoint failed with status {}: {}",
        status,
        body_text
    );

    let search_results: Vec<SearchResponseFile> =
        serde_json::from_str(&body_text).unwrap_or_else(|e| {
            panic!(
                "Failed to parse /search response: {}. Body: {}",
                e, body_text
            )
        });

    assert!(
        !search_results.is_empty(),
        "Expected search results for query '{}', got none. Results: {:?}",
        search_query_text,
        search_results
    );

    // Check if the processed file is among the top results
    let found_target_file = search_results.iter().any(|res| res.path == file_path_str);
    assert!(
        found_target_file,
        "The processed file '{}' was not found in the search results for query '{}'. Results: {:?}",
        file_path_str, search_query_text, search_results
    );

    if found_target_file {
        tracing::info!("TEST (Live Ollama): Successfully found processed file in search results.");
        for res in search_results {
            tracing::info!("  Result: Path: {}, Score: {}", res.path, res.score);
        }
    }
}
