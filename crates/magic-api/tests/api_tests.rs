use axum::Router;
use magic_api::{AppState, create_app};
use magic_core::{
    metadata_store::MetadataStore, ollama_client::OllamaClient, vector_store::VectorStore,
};
use reqwest::Client as TestClient;
use serde_json::{Value, json};
use std::env;
use std::fs::{File, create_dir_all};
use std::io::Write;
use std::path::PathBuf;
use std::sync::Arc;
use tempfile::tempdir;
use tokio::net::TcpListener;

// --- Configuration for Live Ollama Tests ---
// You should have these models pulled in your Ollama instance:
const TEST_OLLAMA_EMBEDDING_MODEL: &str = "mxbai-embed-large";

// Helper to spawn the app with a specific test state, configured for live Ollama
async fn spawn_test_app_with_live_ollama() -> String {
    let listener = TcpListener::bind("127.0.0.1:0")
        .await
        .expect("Failed to bind random port");
    let app_address = format!("http://{}", listener.local_addr().unwrap());

    let temp_data_dir = tempdir().expect("Failed to create temp dir for test data");
    let metadata_db_path = temp_data_dir.path().join("test_api_meta_live.sqlite");
    let vector_db_path_dir = temp_data_dir.path().join("test_api_vector_data_live");
    create_dir_all(&vector_db_path_dir).expect("Failed to create test vector DB dir");

    // Read Ollama URL from environment variable or use default
    let ollama_url = env::var("INTELLIFOLDER_TEST_OLLAMA_URL")
        .unwrap_or_else(|_| "http://localhost:11434".to_string());

    tracing::info!("Test App connecting to Ollama at: {}", ollama_url);
    tracing::info!("Using embedding model: {}", TEST_OLLAMA_EMBEDDING_MODEL);

    let ollama_client = Arc::new(OllamaClient::new(
        ollama_url,
        TEST_OLLAMA_EMBEDDING_MODEL.to_string(),
    ));
    let metadata_store = Arc::new(
        MetadataStore::new(&metadata_db_path)
            .await
            .expect("Failed to init test metadata store"),
    );
    let vector_store = Arc::new(
        VectorStore::new(&vector_db_path_dir, "api_test_files_live")
            .await
            .expect("Failed to init test vector store"),
    );

    let test_app_state = AppState {
        ollama_client,
        metadata_store,
        vector_store,
    };

    let app: Router = create_app(test_app_state);

    tokio::spawn(async move {
        axum::serve(listener, app.into_make_service())
            .await
            .unwrap();
        let _keep_temp_dir = temp_data_dir;
    });

    app_address
}

#[tokio::test]
#[ignore = "Requires a running Ollama instance with specific models"] // See note below
async fn test_process_file_and_search_with_live_ollama() {
    // Ensure Ollama is running and TEST_OLLAMA_EMBEDDING_MODEL is pulled.
    // You might want to add a check here or rely on the test runner's environment.
    // For example, try a quick ping to Ollama or list models.

    let app_address = spawn_test_app_with_live_ollama().await;
    let client = TestClient::new();

    let temp_file_dir = tempdir().unwrap();
    let file_to_process = temp_file_dir.path().join("live_search_test.txt");
    let mut file = File::create(&file_to_process).unwrap();
    let file_content =
        "This is a document about the Rust programming language and building AI applications.";
    writeln!(file, "{}", file_content).unwrap();
    let file_path_str = file_to_process.to_str().unwrap().to_string();

    // 1. Process the file
    tracing::info!("TEST: Processing file: {}", file_path_str);
    let process_response = client
        .post(format!("{}/process_file", app_address))
        .json(&json!({ "file_path": file_path_str }))
        .send()
        .await
        .expect("Failed to call /process_file");

    let process_status = process_response.status();
    let process_body_text = process_response.text().await.unwrap_or_default();
    assert!(
        process_status.is_success(),
        "Process file failed with status {}: {}",
        process_status,
        process_body_text
    );
    let process_body: Value =
        serde_json::from_str(&process_body_text).expect("Failed to parse process response");
    assert_eq!(process_body["message"], "File processed successfully");

    // Give a moment for backend processing (especially if it involves async writes after Ollama call)
    tokio::time::sleep(tokio::time::Duration::from_secs(1)).await; // Increased for live Ollama

    // 2. Search for the file
    let search_query = "AI applications in Rust";
    tracing::info!("TEST: Searching for query: '{}'", search_query);
    let search_response = client
        .post(format!("{}/search", app_address))
        .json(&json!({ "query": search_query, "top_k": 1 }))
        .send()
        .await
        .expect("Failed to call /search");

    let search_status = search_response.status();
    let search_body_text = search_response.text().await.unwrap_or_default();
    assert!(
        search_status.is_success(),
        "Search endpoint failed with status {}: {}",
        search_status,
        search_body_text
    );
    let search_results: Vec<Value> =
        serde_json::from_str(&search_body_text).expect("Failed to parse search results");

    assert!(
        !search_results.is_empty(),
        "Expected at least one search result for query '{}'. Results: {:?}",
        search_query,
        search_results
    );
    // With live Ollama, the top result should be our processed file.
    // The exact score will vary, so we focus on the path.
    let found_target_file = search_results
        .iter()
        .any(|res| res["path"] == file_path_str);
    assert!(
        found_target_file,
        "The processed file was not found in the search results for query '{}'",
        search_query
    );

    if !search_results.is_empty() {
        tracing::info!(
            "TEST: Top search result path: {:?}, score: {:?}",
            search_results[0]["path"],
            search_results[0]["score"]
        );
    }
}

// You can add more tests here that interact with the live Ollama instance.
// For example, testing different types of queries, or how your app handles
// Ollama errors if you can reliably trigger them (e.g., by specifying a non-existent model).
