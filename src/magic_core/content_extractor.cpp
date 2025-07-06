#include "magic_core/content_extractor.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
namespace magic_core {

ContentExtractor::ContentExtractor() {
  // Initialize supported file extensions
  supported_extensions_ = {".txt", ".md",   ".markdown", ".rst", ".cpp", ".cc",   ".cxx",
                           ".hpp", ".h",    ".c",        ".py",  ".js",  ".ts",   ".java",
                           ".cs",  ".php",  ".rb",       ".go",  ".rs",  ".html", ".htm",
                           ".xml", ".json", ".yaml",     ".yml", ".log", ".csv",  ".tsv"};
}

ContentExtractor::~ContentExtractor() = default;

ContentExtractor::ContentExtractor(ContentExtractor &&other) noexcept
    : supported_extensions_(std::move(other.supported_extensions_)) {}

ContentExtractor &ContentExtractor::operator=(ContentExtractor &&other) noexcept {
  if (this != &other) {
    supported_extensions_ = std::move(other.supported_extensions_);
  }
  return *this;
}

ExtractedContent ContentExtractor::extract_content(const std::filesystem::path &file_path) {
  if (!std::filesystem::exists(file_path)) {
    throw ContentExtractorError("File does not exist: " + file_path.string());
  }

  FileType file_type = detect_file_type(file_path);

  switch (file_type) {
    case FileType::Text:
      return extract_text_file(file_path);
    case FileType::Markdown:
      return extract_markdown_file(file_path);
    case FileType::Code:
      return extract_code_file(file_path);
    case FileType::PDF:
      return extract_pdf_file(file_path);
    default:
      throw ContentExtractorError("Unsupported file type: " + file_path.string());
  }
}

ExtractedContent ContentExtractor::extract_from_text(const std::string &text,
                                                     const std::string &filename) {
  ExtractedContent content;
  content.text_content = text;
  content.title = extract_title_from_content(text);
  content.keywords = extract_keywords(text);
  content.file_type = FileType::Text;
  content.word_count = count_words(text);

  return content;
}

/*TODO
 * This needs to be a regex
 */
FileType ContentExtractor::detect_file_type(const std::filesystem::path &file_path) {
  std::string extension = file_path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if (extension == ".pdf") {
    return FileType::PDF;
  } else if (extension == ".md" || extension == ".markdown") {
    return FileType::Markdown;
  } else if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" ||
             extension == ".hpp" || extension == ".h" || extension == ".c" || extension == ".py" ||
             extension == ".js" || extension == ".ts" || extension == ".java" ||
             extension == ".cs" || extension == ".php" || extension == ".rb" ||
             extension == ".go" || extension == ".rs") {
    return FileType::Code;
  } else {
    return FileType::Text;
  }
}

bool ContentExtractor::is_supported_file_type(const std::filesystem::path &file_path) {
  std::string extension = file_path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  return std::find(supported_extensions_.begin(), supported_extensions_.end(), extension) !=
         supported_extensions_.end();
}

std::vector<std::string> ContentExtractor::get_supported_extensions() const {
  return supported_extensions_;
}

ExtractedContent ContentExtractor::extract_text_file(const std::filesystem::path &file_path) {
  std::string content = read_file_content(file_path);

  ExtractedContent extracted;
  extracted.text_content = content;
  extracted.title = extract_title_from_content(content);
  extracted.keywords = extract_keywords(content);
  extracted.file_type = FileType::Text;
  extracted.word_count = count_words(content);

  return extracted;
}

ExtractedContent ContentExtractor::extract_pdf_file(const std::filesystem::path &file_path) {
  // TODO: Implement PDF extraction using a library like poppler or pdf2text
  throw ContentExtractorError("PDF extraction not yet implemented");
}

ExtractedContent ContentExtractor::extract_markdown_file(const std::filesystem::path &file_path) {
  std::string content = read_file_content(file_path);

  ExtractedContent extracted;
  extracted.text_content = content;
  extracted.title = extract_title_from_content(content);
  extracted.keywords = extract_keywords(content);
  extracted.file_type = FileType::Markdown;
  extracted.word_count = count_words(content);

  return extracted;
}

ExtractedContent ContentExtractor::extract_code_file(const std::filesystem::path &file_path) {
  std::string content = read_file_content(file_path);

  ExtractedContent extracted;
  extracted.text_content = content;
  extracted.title = file_path.filename().string();
  extracted.keywords = extract_keywords(content);
  extracted.file_type = FileType::Code;
  extracted.word_count = count_words(content);

  return extracted;
}

std::string ContentExtractor::read_file_content(const std::filesystem::path &file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    throw ContentExtractorError("Failed to open file: " + file_path.string());
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string ContentExtractor::extract_title_from_content(const std::string &content) {
  // Simple title extraction - first non-empty line
  std::istringstream iss(content);
  std::string line;

  while (std::getline(iss, line)) {
    // Remove leading/trailing whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    if (!line.empty()) {
      // For markdown, remove # symbols
      if (line[0] == '#') {
        line.erase(0, line.find_first_not_of("# "));
      }
      return line;
    }
  }

  return "Untitled";
}

std::vector<std::string> ContentExtractor::extract_keywords(const std::string &content) {
  // Simple keyword extraction - words that appear multiple times
  std::map<std::string, int> word_count;
  std::regex word_regex(R"(\b\w+\b)");

  auto words_begin = std::sregex_iterator(content.begin(), content.end(), word_regex);
  auto words_end = std::sregex_iterator();

  for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
    std::string word = (*i).str();
    std::transform(word.begin(), word.end(), word.begin(), ::tolower);

    // Skip common words
    if (word.length() > 3 && word != "the" && word != "and" && word != "for" && word != "with" &&
        word != "this" && word != "that" && word != "from") {
      word_count[word]++;
    }
  }

  // Get top 10 most frequent words
  std::vector<std::pair<std::string, int>> sorted_words(word_count.begin(), word_count.end());
  std::sort(sorted_words.begin(), sorted_words.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  std::vector<std::string> keywords;
  for (size_t i = 0; i < std::min(size_t(10), sorted_words.size()); ++i) {
    keywords.push_back(sorted_words[i].first);
  }

  return keywords;
}

size_t ContentExtractor::count_words(const std::string &text) {
  std::regex word_regex(R"(\b\w+\b)");
  auto words_begin = std::sregex_iterator(text.begin(), text.end(), word_regex);
  auto words_end = std::sregex_iterator();

  return std::distance(words_begin, words_end);
}
std::string ContentExtractor::compute_content_hash(const std::filesystem::path &file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    throw ContentExtractorError("Failed to open file for hashing: " + file_path.string());
  }

  // Use modern EVP API instead of deprecated SHA256_* functions
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    throw ContentExtractorError("Failed to create EVP context for hashing");
  }

  if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw ContentExtractorError("Failed to initialize SHA256 digest");
  }

  char buffer[1024];
  while (file.read(buffer, sizeof(buffer))) {
    if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
      EVP_MD_CTX_free(mdctx);
      throw ContentExtractorError("Failed to update SHA256 digest");
    }
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw ContentExtractorError("Failed to finalize SHA256 digest");
  }

  EVP_MD_CTX_free(mdctx);

  std::stringstream ss;
  for (unsigned int i = 0; i < hash_len; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }

  return ss.str();
}

}  // namespace magic_core