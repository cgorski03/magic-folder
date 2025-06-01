use arrow_array::RecordBatchIterator;
use arrow_array::{Array, ArrayRef, FixedSizeListArray, Float32Array, RecordBatch, StringArray};
use arrow_schema::{DataType, Field, Schema, SchemaRef};
use lancedb::Connection;
use lancedb::Result;
use lancedb::query::QueryBase;
use lancedb::query::Select::Columns;
use std::path::Path;
use std::sync::Arc;
// Match to output dim
const EMBEDDING_DIM: i32 = 1024;
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

        // Create vector array
        let vector_values_array: Float32Array = Float32Array::from(vector);
        let values_array_ref: ArrayRef = Arc::new(vector_values_array);

        // Create the fixed size list array with the correct field for inner items
        let inner_field = Arc::new(Field::new("item", DataType::Float32, true));
        let list_array = FixedSizeListArray::try_new(
            inner_field,   // The field for the inner Float32 items
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

        tbl.add(batch_reader).execute().await?;

        Ok(())
    }

    pub async fn search_similar(
        &self,
        query_vector: &[f32],
        top_k: usize,
    ) -> Result<Vec<(String, f32)>> {
        let tbl = self.conn.open_table(&self.table_name).execute().await?;

        let results = lancedb::query::ExecutableQuery::execute(
            &tbl.vector_search(query_vector)?
                .limit(top_k)
                // Only thing we need is the path
                .select(Columns(vec![String::from("path")])),
        )
        .await?;

        let mut similar_files = Vec::new();
        use futures::TryStreamExt;
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
