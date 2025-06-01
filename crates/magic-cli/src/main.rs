use clap::Parser;
use reqwest::Client;
use serde_json::json;

const API_BASE_URL: &str = "http://localhost:3030";

#[derive(Parser, Debug)]
#[clap(author, version, about, long_about = None)]
struct Cli {
    #[clap(subcommand)]
    command: Commands,
}

#[derive(Parser, Debug)]
enum Commands {
    /// Process a single file
    Process {
        /// Path to the file to process
        #[clap(short, long)]
        file: String,
    },
    /// Search for files based on a query
    Search {
        /// The search query
        #[clap(short, long)]
        query: String,
        /// Number of results to return
        #[clap(short, long, default_value_t = 5)]
        top_k: usize,
    },
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt::init(); // Basic tracing for CLI

    let cli = Cli::parse();
    let client = Client::new();

    match cli.command {
        Commands::Process { file } => {
            tracing::info!("Requesting processing for file: {}", file);
            let response = client
                .post(format!("{}/process_file", API_BASE_URL))
                .json(&json!({ "file_path": file }))
                .send()
                .await?;

            if response.status().is_success() {
                let response_body: serde_json::Value = response.json().await?;
                println!("Successfully processed file:\n{}", serde_json::to_string_pretty(&response_body)?);
            } else {
                eprintln!("Error processing file: {} - {}", response.status(), response.text().await?);
            }
        }
        Commands::Search { query, top_k } => {
            tracing::info!("Searching for: '{}', top_k: {}", query, top_k);
            let response = client
                .post(format!("{}/search", API_BASE_URL))
                .json(&json!({ "query": query, "top_k": top_k }))
                .send()
                .await?;

            if response.status().is_success() {
                let search_results: Vec<serde_json::Value> = response.json().await?;
                println!("Search results:\n{}", serde_json::to_string_pretty(&search_results)?);
            } else {
                eprintln!("Error searching: {} - {}", response.status(), response.text().await?);
            }
        }
    }
    Ok(())
}