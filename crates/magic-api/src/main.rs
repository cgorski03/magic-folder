use dotenvy::dotenv;
use magic_api::create_app;
use once_cell::sync::Lazy;
use std::env;
use std::path::PathBuf;
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt};

static WATCHED_FOLDER: Lazy<String> =
    Lazy::new(|| env::var("WATCHED_FOLDER").expect("WATCHED_FOLDER must be set"));
static API_BASE_URL: Lazy<String> =
    Lazy::new(|| env::var("API_BASE_URL").expect("API_BASE_URL must be set"));
static VECTOR_DB_PATH: Lazy<String> =
    Lazy::new(|| env::var("VECTOR_DB_PATH").expect("VECTOR_DB_PATH must be set"));
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

    match dotenv().ok() {
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
