// crates/intelli-api/src/main.rs
use axum::{
    Json, Router,
    extract::State,
    http::StatusCode,
    routing::{get, post},
};
use magic_core::{
    content_extractor, metadata_store::MetadataStore, ollama_client::OllamaClient,
    vector_store::VectorStore,
};
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use std::{env, path::PathBuf};
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt};

// --- Configuration using once_cell::sync::Lazy ---
static OLLAMA_URL: Lazy<String> =
    Lazy::new(|| env::var("OLLAMA_URL").expect("OLLAMA_URL must be set"));
static EMBEDDING_MODEL: Lazy<String> =
    Lazy::new(|| env::var("EMBEDDING_MODEL").expect("EMBEDDING_MODEL must be set"));
static METADATA_DB_PATH: Lazy<String> =
    Lazy::new(|| env::var("METADATA_DB_PATH").expect("METADATA_DB_PATH must be set"));
static VECTOR_DB_PATH: Lazy<String> =
    Lazy::new(|| env::var("VECTOR_DB_PATH").expect("VECTOR_DB_PATH must be set"));
static WATCHED_FOLDER: Lazy<String> =
    Lazy::new(|| env::var("WATCHED_FOLDER").expect("WATCHED_FOLDER must be set"));
static API_BASE_URL: Lazy<String> =
    Lazy::new(|| env::var("API_BASE_URL").expect("API_BASE_URL must be set"));

#[derive(Clone)]
pub struct AppState {
    ollama_client: Arc<OllamaClient>,
    metadata_store: Arc<MetadataStore>,
    vector_store: Arc<VectorStore>,
}

// Lifting this logic out of the main function makes it easier to test
pub async fn create_app(custom_state: Option<AppState>) -> Router {
    let app_state = if let Some(state) = custom_state {
        state
    } else {
        // When accessing Lazy statics, you need to dereference them (e.g., &*OLLAMA_URL or OLLAMA_URL.clone())
        let ollama_client = Arc::new(OllamaClient::new(
            OLLAMA_URL.clone(),      // Clone the String from Lazy
            EMBEDDING_MODEL.clone(), // Clone the String from Lazy
        ));
        let metadata_store = Arc::new(
            MetadataStore::new(PathBuf::from(METADATA_DB_PATH.as_str()).as_path()) // Use .as_str() or .clone()
                .await
                .expect("Failed to init metadata store"),
        );
        let vector_store = Arc::new(
            VectorStore::new(PathBuf::from(VECTOR_DB_PATH.as_str()).as_path(), "files") // Use .as_str() or .clone()
                .await
                .expect("Failed to init vector store"),
        );
        AppState {
            ollama_client,
            metadata_store,
            vector_store,
        }
    };

    let app = Router::new()
        .route("/", get(root))
        .route("/process_file", post(process_file_handler))
        .route("/search", post(search_handler))
        .with_state(app_state)
        .layer(tower_http::trace::TraceLayer::new_for_http())
        .layer(
            tower_http::cors::CorsLayer::new()
                .allow_origin(tower_http::cors::Any)
                .allow_methods(tower_http::cors::Any)
                .allow_headers(tower_http::cors::Any),
        );
    app
}

#[tokio::main]
async fn main() {
    // if we don't init tracing, we won't be able to see the env results
    tracing_subscriber::registry()
        .with(tracing_subscriber::EnvFilter::new(
            std::env::var("RUST_LOG")
                .unwrap_or_else(|_| "intelli_api=debug,tower_http=debug,magic_core=info".into()),
        ))
        .with(tracing_subscriber::fmt::layer())
        .init();

    // Now that tracing is initialized, we can log the dotenv result
    match dotenvy::dotenv().ok() {
        // Call it again or store the result from the first call
        Some(path) => tracing::info!(".env file loaded successfully from: {:?}", path),
        None => tracing::warn!(
            "Could not load .env file or it was already loaded. This is fine if variables are set externally."
        ),
    }

    tracing::info!("Ensuring vector DB path exists: {}", &*VECTOR_DB_PATH);
    std::fs::create_dir_all(VECTOR_DB_PATH.as_str()).expect("Failed to create vector DB path");

    tracing::info!("Ensuring watched folder exists: {}", &*WATCHED_FOLDER);
    let watched_path = PathBuf::from(WATCHED_FOLDER.as_str());
    std::fs::create_dir_all(&watched_path).expect("Failed to create watched folder");

    let app = create_app(None).await;

    let listener = tokio::net::TcpListener::bind(API_BASE_URL.as_str())
        .await
        .unwrap();
    tracing::debug!("listening on {}", listener.local_addr().unwrap());
    axum::serve(listener, app).await.unwrap();
}

pub async fn root() -> &'static str {
    "MagicFolder API is running!"
}

#[derive(Deserialize)]
pub struct ProcessFileRequest {
    // Made pub
    file_path: String,
}

#[derive(Serialize)]
pub struct ProcessFileResponse {
    // Made pub
    message: String,
    file_id: Option<i64>,
    vector_id: Option<String>,
}

pub async fn process_file_handler(
    // Made pub
    State(state): State<AppState>,
    Json(payload): Json<ProcessFileRequest>,
) -> Result<Json<ProcessFileResponse>, (StatusCode, String)> {
    tracing::info!("Processing file: {}", payload.file_path);
    let path = PathBuf::from(&payload.file_path);

    if !path.exists() {
        return Err((
            StatusCode::BAD_REQUEST,
            format!("File not found: {}", payload.file_path),
        ));
    }

    let text_content = content_extractor::extract_text_from_file(&path).map_err(|e| {
        (
            StatusCode::INTERNAL_SERVER_ERROR,
            format!("Extraction error: {}", e),
        )
    })?;

    if text_content.is_empty() {
        tracing::warn!(
            "No text extracted or unsupported file: {}",
            payload.file_path
        );
        return Ok(Json(ProcessFileResponse {
            message: "No text extracted or unsupported file type".to_string(),
            file_id: None,
            vector_id: None,
        }));
    }

    let embedding = state
        .ollama_client
        .get_embedding(&text_content)
        .await
        .map_err(|e| {
            (
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("Ollama embedding error: {}", e),
            )
        })?;

    let vector_id_str = path.to_string_lossy().into_owned();

    state
        .vector_store
        .add_embedding(&vector_id_str, embedding)
        .await
        .map_err(|e| {
            (
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("Vector store error: {}", e),
            )
        })?;

    let metadata_id = state
        .metadata_store
        .add_file_metadata(&payload.file_path, &vector_id_str)
        .await
        .map_err(|e| {
            (
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("Metadata store error: {}", e),
            )
        })?;

    Ok(Json(ProcessFileResponse {
        message: "File processed successfully".to_string(),
        file_id: Some(metadata_id),
        vector_id: Some(vector_id_str),
    }))
}

#[derive(Deserialize)]
pub struct SearchRequest {
    // Made pub
    query: String,
    top_k: Option<usize>,
}

#[derive(Serialize)]
pub struct SearchResponseFile {
    // Made pub
    path: String,
    score: f32,
}

pub async fn search_handler(
    State(state): State<AppState>,
    Json(payload): Json<SearchRequest>,
) -> Result<Json<Vec<SearchResponseFile>>, (StatusCode, String)> {
    let query_embedding = state
        .ollama_client
        .get_embedding(&payload.query)
        .await
        .map_err(|e| {
            (
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("Ollama query embedding error: {}", e),
            )
        })?;

    let top_k = payload.top_k.unwrap_or(5);
    let results = state
        .vector_store
        .search_similar(&query_embedding, top_k)
        .await
        .map_err(|e| {
            (
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("Vector search error: {}", e),
            )
        })?;

    let response_files = results
        .into_iter()
        .map(|(path, score)| SearchResponseFile { path, score })
        .collect();

    Ok(Json(response_files))
}
