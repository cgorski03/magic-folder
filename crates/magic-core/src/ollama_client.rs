use reqwest::Client;
use serde::Deserialize;
use serde_json::json;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum OllamaError {
    #[error("Reqwest error: {0}")]
    Reqwest(#[from] reqwest::Error),
    #[error("Ollama API error: {0}")]
    ApiError(String),
}

#[derive(Deserialize, Debug)]
struct EmbeddingResponse {
    embedding: Vec<f32>,
}

pub struct OllamaClient {
    client: Client,
    ollama_url: String,
    embedding_model: String,
}

impl OllamaClient {
    pub fn new(ollama_url: String, embedding_model: String) -> Self {
        Self {
            client: Client::new(),
            ollama_url,
            embedding_model,
        }
    }

    pub async fn get_embedding(&self, text: &str) -> Result<Vec<f32>, OllamaError> {
        let response = self
            .client
            .post(format!("{}/api/embeddings", self.ollama_url))
            .json(&json!({
                "model": self.embedding_model,
                "prompt": text
            }))
            .send()
            .await?;

        if !response.status().is_success() {
            let error_text = response.text().await.unwrap_or_default();
            return Err(OllamaError::ApiError(format!(
                "Ollama API request failed: {}",
                error_text
            )));
        }
        let embedding_response = response.json::<EmbeddingResponse>().await?;
        Ok(embedding_response.embedding)
    }
}