use std::fs;
use std::io;
use std::path::Path;

pub fn extract_text_from_file(file_path: &Path) -> io::Result<String> {
    // Basic for .txt, .md. Expand later for PDF, DOCX etc.
    // TODO: Expand later for PDF, DOCX etc.
    let extension = file_path.extension().and_then(|s| s.to_str());
    match extension {
        Some("txt") | Some("md") => fs::read_to_string(file_path),
        _ => Ok(String::new()), // Or an error for unsupported types
    }
}