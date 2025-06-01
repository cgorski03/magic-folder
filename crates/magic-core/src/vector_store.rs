use lancedb::connection::Connection;
use lancedb::table::AddDataOptions;
use lancedb::Result;
use arrow_array::{FixedSizeListArray, Float32Array, RecordBatch, StringArray};
use arrow_schema::{DataType, Field, Schema};
use std::sync::Arc;
use std::path::Path;

// Match to output dim
const EMBEDDING_DIM: i32 = 768;

pub struct VectorStore {
    conn: Arc<dyn Connection>,
    table_name: String,
}

impl VectorStore {
    pub async fn new(db_path: &Path, table_name: &str) -> Result<Self> {
        let uri = db_path.to_str().unwrap();
        let conn = lancedb::connect(uri).execute().await?;
        let schema = Arc::new(Schema::new(vec![
            // Store path for reference
            Field::new("path", DataType::Utf8, false),
            Field::new(
                "vector",
                DataType::FixedSizeList(Arc::new(Field::new("item", DataType::Float32, true)), EMBEDDING_DIM),
                true,
            ),
        ]));

        // Try to open table, create if not exists (LanceDB handles this)
        let _ = conn.create_table(table_name, schema)
            .mode(lancedb::table::TableCreateMode::Create)
            .execute().await;


        Ok(Self { conn: Arc::from(conn), table_name: table_name.to_string() })
    }

    pub async fn add_embedding(&self, path: &str, vector: Vec<f32>) -> Result<()> {
        let tbl = self.conn.open_table(&self.table_name).execute().await?;
        // Get schema from existing table
        let schema = tbl.schema().await?;
        let path_array = StringArray::from(vec![path]);
        let vector_array = Float32Array::from(vector);
        let list_array = FixedSizeListArray::try_new_from_values(vector_array, EMBEDDING_DIM).unwrap();

        let batch = RecordBatch::try_new(
          // Use the table's schema
          schema.clone(),
            vec![Arc::new(path_array), Arc::new(list_array)],
        )?;

        tbl.add(Arc::new(vec![batch]), AddDataOptions::default()).await?;
        Ok(())
    }

    pub async fn search_similar(&self, query_vector: &[f32], top_k: usize) -> Result<Vec<(String, f32)>> {
        let tbl = self.conn.open_table(&self.table_name).execute().await?;
        let results = tbl
            .search(query_vector)
            .limit(top_k)
            // Only thing we need is the path
            .select(&["path"])
            // Use stream for if there are a ton of results
            .execute_stream() 
            .await?;

        let mut similar_files = Vec::new();
        use futures::stream::StreamExt;
        let batches: Vec<RecordBatch> = results.try_collect().await?;

        for batch in batches {
            let path_col = batch
                .column_by_name("path")
                .unwrap()
                .as_any()
                .downcast_ref::<StringArray>()
                .unwrap();
            let distance_col = batch
                // LanceDB adds this column in results
                .column_by_name("_distance") 
                .unwrap()
                .as_any()
                .downcast_ref::<Float32Array>()
                .unwrap();

            for i in 0..batch.num_rows() {
                similar_files.push((path_col.value(i).to_string(), distance_col.value(i)));
            }
        }
        Ok(similar_files)
    }
}