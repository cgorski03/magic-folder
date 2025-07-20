#include "magic_core/extractors/content_extractor.hpp"
#include <openssl/evp.h>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace magic_core {

std::string ContentExtractor::get_content_hash(const fs::path& file_path) const {
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
          throw ContentExtractorError("Failed to open file for hashing: " + file_path.string());
        }
      
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
        
        // Process any remaining bytes (for files smaller than buffer size)
        if (file.gcount() > 0) {
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
}