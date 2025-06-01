use arrow_array::RecordBatchIterator;
use arrow_array::{ArrayRef, FixedSizeListArray, Float32Array, RecordBatch, StringArray};
use arrow_schema::{DataType, Field, FieldRef, Schema, SchemaRef};
use futures::stream::StreamExt;
use lancedb::Result;
use lancedb::connection::Connection;
use lancedb::query::QueryBase;
use std::path::Path;
use std::sync::Arc;

// Match to output dim
const EMBEDDING_DIM: i32 = 768;
const EMBEDDING_FIELD_NAME: &str = "vector";

pub struct VectorStore {
    conn: Arc<Connection>,
    schema: SchemaRef,
    table_name: String,
}

impl VectorStore {
    pub async fn new(db_path: &Path, table_name: &str) -> Result<Self> {
        let uri = db_path.to_str().unwrap();
        let conn = lancedb::connect(uri).execute().await?;

        // Define the schema we want
        let schema: SchemaRef = Arc::new(Schema::new(vec![
            Field::new("path", DataType::Utf8, false),
            Field::new(
                EMBEDDING_FIELD_NAME,
                DataType::FixedSizeList(
                    Arc::new(Field::new("item", DataType::Float32, true)),
                    EMBEDDING_DIM,
                ),
                true,
            ),
        ]));

        // if table already exists
        let table_names = conn.table_names().execute().await?;

        if !table_names.contains(&table_name.to_string()) {
            // Create table with our schema
            conn.create_empty_table(table_name, schema.clone())
                .execute()
                .await?;
        }

        Ok(Self {
            conn: Arc::from(conn),
            schema: schema,
            table_name: table_name.to_string(),
        })
    }

    pub async fn add_embedding(&self, path: &str, vector: Vec<f32>) -> Result<()> {
        let tbl = self.conn.open_table(&self.table_name).execute().await?;

        // Create path array
        let path_array = StringArray::from(vec![path]);
        let path_array_ref: ArrayRef = Arc::new(path_array);

        // Get the field reference for the vector column
        let list_field_ref = self.schema.field_with_name(EMBEDDING_FIELD_NAME)?;
        let field_arc: FieldRef = Arc::new(list_field_ref.clone());

        // Create vector array
        let vector_values_array: Float32Array = Float32Array::from(vector);
        let values_array_ref: ArrayRef = Arc::new(vector_values_array);

        // Create the fixed size list array
        let list_array = FixedSizeListArray::try_new(
            field_arc,     // The schema for this FSL column
            EMBEDDING_DIM, // The fixed size of each list
            values_array_ref,
            None,
        )?;

        let list_array_ref: ArrayRef = Arc::new(list_array);

        // Create the record batch
        let batch: RecordBatch =
            RecordBatch::try_new(self.schema.clone(), vec![path_array_ref, list_array_ref])?;

        // Create a RecordBatchIterator
        let batches = vec![Ok(batch)];
        let batch_reader = RecordBatchIterator::new(batches.into_iter(), self.schema.clone());

        tbl.add(batch_reader);

        Ok(())
    }

    pub async fn search_similar(
        &self,
        query_vector: &[f32],
        top_k: usize,
    ) -> Result<Vec<(String, f32)>> {
        let tbl = self.conn.open_table(&self.table_name).execute().await?;

        let results = tbl
            .vector_search(query_vector)?
            .limit(top_k)
            // Only thing we need is the path
            .select(&["path"])
            // Use stream for if there are a ton of results
            .execute_stream()
            .await?;

        let mut similar_files = Vec::new();
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
