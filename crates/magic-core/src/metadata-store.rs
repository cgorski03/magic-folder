use rusqlite::{params, Connection, Result};
use std::path::Path;
use std::sync::Arc;
use tokio::sync::Mutex;

pub struct MetadataStore {
    // Wrap connection for async access
    conn: Arc<Mutex<Connection>>,
}

impl MetadataStore {
    pub async fn new(db_path: &Path) -> cResult<Self> {
        let conn = Connection::open(db_path)?;
        conn.execute(
            "CREATE TABLE IF NOT EXISTS files (
                id INTEGER PRIMARY KEY,
                path TEXT NOT NULL UNIQUE,
                processed_at TEXT NOT NULL,
                vector_id TEXT UNIQUE -- Could be LanceDB's internal ID or a UUID
            )",
            [],
        )?;
        Ok(Self { conn: Arc::new(Mutex::new(conn)) })
    }

    pub async fn add_file_metadata(&self, path: &str, vector_id: &str) -> Result<i64> {
        let conn = self.conn.lock().await;
        let now = chrono::Utc::now().to_rfc3339();
        conn.execute(
            "INSERT OR REPLACE INTO files (path, processed_at, vector_id) VALUES (?1, ?2, ?3)",
            params![path, now, vector_id],
        )?;
        Ok(conn.last_insert_rowid())
    }
    // TODO: Add other methods like get_file_by_path, get_all_files etc.
}
