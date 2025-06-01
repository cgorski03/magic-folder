use lancedb::Connection;
use std::path::PathBuf;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Vector Store Inspection");
    println!("======================");

    // Connect to the database
    let uri = "data/vector_data";
    let conn = lancedb::connect(uri).execute().await?;

    // List tables
    let table_names = conn.table_names().execute().await?;
    println!("Tables found: {:?}", table_names);

    if table_names.contains(&"files".to_string()) {
        let tbl = conn.open_table("files").execute().await?;

        // Get schema
        let schema = tbl.schema().await?;
        println!("\nTable 'files' schema:");
        for field in schema.fields() {
            println!("  - {}: {:?}", field.name(), field.data_type());
        }

        // Get row count
        let count = tbl.count_rows(None).await?;
        println!("\nTotal rows: {}", count);
    } else {
        println!("No 'files' table found");
    }

    Ok(())
}
