use axum::{routing::{get, post}, Router, Json, extract::State, http::StatusCode};
use magic_core::{
    ollama_client::OllamaClient,
    metadata_store::MetadataStore,
    vector_store::VectorStore,
    content_extractor,
};
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use std::path::PathBuf;
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt};

// --- Configuration ---
const OLLAMA_URL: &str = "http://localhost:11434";
const EMBEDDING_MODEL: &str = "mxbai-embed-large";
const METADATA_DB_PATH: &str = "data/metadata.sqlite";
const VECTOR_DB_PATH: &str = "data/vector_data";
const WATCHED_FOLDER: &str = "./watched_files";

#[derive(Clone)]
struct AppState {
    ollama_client: Arc<OllamaClient>,
    metadata_store: Arc<MetadataStore>,
    vector_store: Arc<VectorStore>,
}

#[tokio::main]
async fn main() {
    tracing_subscriber::registry()
        .with(tracing_subscriber::EnvFilter::new(
            std::env::var("RUST_LOG").unwrap_or_else(|_| "intelli_api=debug,tower_http=debug".into()),
        ))
        .with(tracing_subscriber::fmt::layer())
        .init();

    // Ensure data directories exist
    std::fs::create_dir_all(VECTOR_DB_PATH).expect("Failed to create vector DB path");
    let watched_path = PathBuf::from(WATCHED_FOLDER);
    std::fs::create_dir_all(&watched_path).expect("Failed to create watched folder");


    let ollama_client = Arc::new(OllamaClient::new(OLLAMA_URL.to_string(), EMBEDDING_MODEL.to_string()));
    let metadata_store = Arc::new(
        MetadataStore::new(PathBuf::from(METADATA_DB_PATH).as_path())
            .await
            .expect("Failed to init metadata store"),
    );
    let vector_store = Arc::new(
        VectorStore::new(PathBuf::from(VECTOR_DB_PATH).as_path(), "files")
            .await
            .expect("Failed to init vector store"),
    );

    let app_state = AppState {
        ollama_client,
        metadata_store,
        vector_store,
    };

    // --- TODO: Start file watcher as a background task ---
    // This needs careful handling with async tasks and channels
    // For now, we'll have an endpoint to manually process a file.
    // let event_rx = intelli_core::file_watcher::watch_folder(watched_path.clone()).unwrap();
    // tokio::spawn(async move {
    //     // Process events from event_rx
    // });


    let app = Router::new()
        .route("/", get(root))
        .route("/process_file", post(process_file_handler)) // Manual trigger for now
        .route("/search", post(search_handler))
        .with_state(app_state)
        .layer(tower_http::trace::TraceLayer::new_for_http())
        .layer(
            tower_http::cors::CorsLayer::new()
                .allow_origin(tower_http::cors::Any)
                .allow_methods(tower_http::cors::Any)
                .allow_headers(tower_http::cors::Any),
        );

    let listener = tokio::net::TcpListener::bind("127.0.0.1:3030") // Your API port
        .await
        .unwrap();
    tracing::debug!("listening on {}", listener.local_addr().unwrap());
    axum::serve(listener, app).await.unwrap();
}

async fn root() -> &'static str {
    "IntelliFolder API is running!"
}

#[derive(Deserialize)]
struct ProcessFileRequest {
    file_path: String, // Relative to some base or absolute
}

#[derive(Serialize)]
struct ProcessFileResponse {
    message: String,
    file_id: Option<i64>, // From metadata DB
    vector_id: Option<String>, // Could be path or LanceDB ID
}

async fn process_file_handler(
    State(state): State<AppState>,
    Json(payload): Json<ProcessFileRequest>,
) -> Result<Json<ProcessFileResponse>, (StatusCode, String)> {
    tracing::info!("Processing file: {}", payload.file_path);
    let path = PathBuf::from(&payload.file_path);

    if !path.exists() {
        return Err((StatusCode::BAD_REQUEST, format!("File not found: {}", payload.file_path)));
    }

    let text_content = content_extractor::extract_text_from_file(&path)
        .map_err(|e| (StatusCode::INTERNAL_SERVER_ERROR, format!("Extraction error: {}", e)))?;

    if text_content.is_empty() {
         tracing::warn!("No text extracted or unsupported file: {}", payload.file_path);
        // Decide how to handle: error, or skip embedding
        return Ok(Json(ProcessFileResponse {
            message: "No text extracted or unsupported file type".to_string(),
            file_id: None,
            vector_id: None,
        }));
    }

    let embedding = state.ollama_client.get_embedding(&text_content).await
        .map_err(|e| (StatusCode::INTERNAL_SERVER_ERROR, format!("Ollama embedding error: {}", e)))?;

    // For LanceDB, the "vector_id" is implicitly handled by the row.
    // We use the path as a unique identifier for now.
    let vector_id_str = path.to_string_lossy().into_owned();

    state.vector_store.add_embedding(&vector_id_str, embedding).await
        .map_err(|e| (StatusCode::INTERNAL_SERVER_ERROR, format!("Vector store error: {}", e)))?;

    let metadata_id = state.metadata_store.add_file_metadata(&payload.file_path, &vector_id_str).await
        .map_err(|e| (StatusCode::INTERNAL_SERVER_ERROR, format!("Metadata store error: {}", e)))?;

    Ok(Json(ProcessFileResponse {
        message: "File processed successfully".to_string(),
        file_id: Some(metadata_id),
        vector_id: Some(vector_id_str),
    }))
}

#[derive(Deserialize)]
struct SearchRequest {
    query: String,
    top_k: Option<usize>,
}

#[derive(Serialize)]
struct SearchResponseFile {
    path: String,
    score: f32, // Similarity score
}

async fn search_handler(
    State(state): State<AppState>,
    Json(payload): Json<SearchRequest>,
) -> Result<Json<Vec<SearchResponseFile>>, (StatusCode, String)> {
    let query_embedding = state.ollama_client.get_embedding(&payload.query).await
        .map_err(|e| (StatusCode::INTERNAL_SERVER_ERROR, format!("Ollama query embedding error: {}", e)))?;

    let top_k = payload.top_k.unwrap_or(5);
    let results = state.vector_store.search_similar(&query_embedding, top_k).await
        .map_err(|e| (StatusCode::INTERNAL_SERVER_ERROR, format!("Vector search error: {}", e)))?;

    let response_files = results.into_iter()
        .map(|(path, score)| SearchResponseFile { path, score })
        .collect();

    Ok(Json(response_files))
}