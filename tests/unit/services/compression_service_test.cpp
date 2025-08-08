#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <unordered_map>

#include "magic_core/services/compression_service.hpp"

namespace magic_core {

class CompressionServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup random number generator for test data
    std::random_device rd;
    rng_.seed(rd());
  }

  void TearDown() override {
    // No cleanup needed for compression service tests
  }

  // Helper to generate random test data
  std::string generate_random_data(size_t size) {
    std::string data(size, '\0');
    std::uniform_int_distribution<int> dist(32, 126); // Printable ASCII
    std::generate(data.begin(), data.end(), [this, &dist]() { return static_cast<char>(dist(rng_)); });
    return data;
  }

  // Helper to generate repetitive data (good for compression)
  std::string generate_repetitive_data(size_t size) {
    std::string pattern = "This is a repetitive pattern that should compress well. ";
    std::string data;
    while (data.size() < size) {
      data += pattern;
    }
    return data.substr(0, size);
  }

  // Helper to generate random binary data
  std::string generate_binary_data(size_t size) {
    std::string data(size, '\0');
    std::uniform_int_distribution<int> dist(0, 255);
    std::generate(data.begin(), data.end(), [this, &dist]() { return static_cast<char>(dist(rng_)); });
    return data;
  }

  // Helper to verify compression/decompression round trip
  void verify_round_trip(const std::string& original_data, int compression_level = 3) {
    // Compress
    std::vector<char> compressed = CompressionService::compress(original_data, compression_level);
    
    // Verify compression actually reduced size (for compressible data)
    // Note: Random/binary data often doesn't compress well and may expand due to overhead
    // Only check compression effectiveness for larger, potentially compressible data
    if (original_data.size() > 1000 && is_likely_compressible(original_data)) {
      EXPECT_LT(compressed.size(), original_data.size()) 
          << "Compression should reduce size for compressible data of size " << original_data.size();
    }
    
    // Decompress
    std::string decompressed = CompressionService::decompress(compressed);
    
    // Verify round trip
    EXPECT_EQ(original_data, decompressed) 
        << "Round trip compression/decompression should preserve original data";
  }

private:
  // Helper to check if data is likely to be compressible
  bool is_likely_compressible(const std::string& data) {
    if (data.size() < 100) return false;
    
    // Check for patterns that indicate compressibility
    std::unordered_map<char, int> char_counts;
    for (char c : data) {
      char_counts[c]++;
    }
    
    // If we have high character repetition, it's likely compressible
    for (const auto& pair : char_counts) {
      if (pair.second > data.size() * 0.1) { // More than 10% repetition of any character
        return true;
      }
    }
    
    // Check for common text patterns (spaces, common letters)
    int text_chars = 0;
    for (char c : data) {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' ' || c == '\n' || c == '\t') {
        text_chars++;
      }
    }
    
    return text_chars > data.size() * 0.5; // More than 50% text-like characters
  }

  std::mt19937 rng_;
};

// Basic functionality tests
TEST_F(CompressionServiceTest, CompressDecompress_EmptyString) {
  std::string empty_data = "";
  verify_round_trip(empty_data);
}

TEST_F(CompressionServiceTest, CompressDecompress_SmallString) {
  std::string small_data = "Hello, World!";
  verify_round_trip(small_data);
}

TEST_F(CompressionServiceTest, CompressDecompress_LargeString) {
  std::string large_data = generate_random_data(10000);
  verify_round_trip(large_data);
}

TEST_F(CompressionServiceTest, CompressDecompress_VeryLargeString) {
  std::string very_large_data = generate_random_data(100000);
  verify_round_trip(very_large_data);
}

// Compression level tests
TEST_F(CompressionServiceTest, CompressDecompress_DifferentCompressionLevels) {
  std::string test_data = generate_repetitive_data(5000);
  
  // Test different compression levels
  for (int level = 1; level <= 22; level += 5) { // Test levels 1, 6, 11, 16, 21
    verify_round_trip(test_data, level);
  }
}

TEST_F(CompressionServiceTest, CompressDecompress_DefaultCompressionLevel) {
  std::string test_data = generate_repetitive_data(3000);
  
  // Test default compression level (3)
  std::vector<char> compressed_default = CompressionService::compress(test_data);
  std::vector<char> compressed_explicit = CompressionService::compress(test_data, 3);
  
  EXPECT_EQ(compressed_default, compressed_explicit) 
      << "Default compression level should be 3";
  
  std::string decompressed = CompressionService::decompress(compressed_default);
  EXPECT_EQ(test_data, decompressed);
}

// Data type tests
TEST_F(CompressionServiceTest, CompressDecompress_RepetitiveData) {
  std::string repetitive_data = generate_repetitive_data(10000);
  verify_round_trip(repetitive_data);
  
  // Verify good compression ratio for repetitive data
  std::vector<char> compressed = CompressionService::compress(repetitive_data);
  double compression_ratio = static_cast<double>(compressed.size()) / repetitive_data.size();
  EXPECT_LT(compression_ratio, 0.5) << "Repetitive data should compress well";
}

TEST_F(CompressionServiceTest, CompressDecompress_RandomData) {
  std::string random_data = generate_random_data(10000);
  verify_round_trip(random_data);
}

TEST_F(CompressionServiceTest, CompressDecompress_BinaryData) {
  std::string binary_data = generate_binary_data(10000);
  verify_round_trip(binary_data);
}

TEST_F(CompressionServiceTest, CompressDecompress_UnicodeData) {
  std::string unicode_data = "Hello, 世界! 🌍 Привет! こんにちは! مرحبا!";
  unicode_data += generate_random_data(5000); // Add some random data too
  verify_round_trip(unicode_data);
}

TEST_F(CompressionServiceTest, CompressDecompress_NewlinesAndSpecialChars) {
  std::string special_data = "Line 1\nLine 2\r\nLine 3\tTabbed\tData\rCarriage Return";
  special_data += std::string(1000, '\0'); // Null bytes
  special_data += std::string(1000, '\xff'); // High bytes
  verify_round_trip(special_data);
}

// Edge cases and error conditions
TEST_F(CompressionServiceTest, CompressDecompress_SingleCharacter) {
  std::string single_char = "A";
  verify_round_trip(single_char);
}

TEST_F(CompressionServiceTest, CompressDecompress_SingleByte) {
  std::string single_byte = "\x42";
  verify_round_trip(single_byte);
}

TEST_F(CompressionServiceTest, CompressDecompress_AllNullBytes) {
  std::string null_data(1000, '\0');
  verify_round_trip(null_data);
}

TEST_F(CompressionServiceTest, CompressDecompress_AllSameByte) {
  std::string same_byte_data(1000, 'X');
  verify_round_trip(same_byte_data);
}

// Performance and compression ratio tests
TEST_F(CompressionServiceTest, CompressionRatio_RepetitiveData) {
  std::string repetitive_data = generate_repetitive_data(50000);
  std::vector<char> compressed = CompressionService::compress(repetitive_data);
  
  double compression_ratio = static_cast<double>(compressed.size()) / repetitive_data.size();
  EXPECT_LT(compression_ratio, 0.3) << "Highly repetitive data should compress very well";
}

TEST_F(CompressionServiceTest, CompressionRatio_RandomData) {
  std::string random_data = generate_random_data(50000);
  std::vector<char> compressed = CompressionService::compress(random_data);
  
  double compression_ratio = static_cast<double>(compressed.size()) / random_data.size();
  // Random data doesn't compress well, but shouldn't expand too much
  EXPECT_LT(compression_ratio, 1.1) << "Random data shouldn't expand too much";
}

// Multiple compression levels comparison
TEST_F(CompressionServiceTest, CompressionLevels_Comparison) {
  std::string test_data = generate_repetitive_data(20000);
  
  std::vector<size_t> compressed_sizes;
  
  // Test compression levels 1, 3, 6, 9, 12, 15, 18, 21
  for (int level : {1, 3, 6, 9, 12, 15, 18, 21}) {
    std::vector<char> compressed = CompressionService::compress(test_data, level);
    compressed_sizes.push_back(compressed.size());
    
    // Verify decompression still works
    std::string decompressed = CompressionService::decompress(compressed);
    EXPECT_EQ(test_data, decompressed);
  }
  
  // Higher compression levels should generally produce smaller output
  // (though this isn't always guaranteed for all data)
  for (size_t i = 1; i < compressed_sizes.size(); ++i) {
    // Allow some tolerance for cases where higher levels don't improve compression
    EXPECT_LE(compressed_sizes[i], compressed_sizes[i-1] * 1.1) 
        << "Higher compression levels should generally produce smaller output";
  }
}

// Stress tests
TEST_F(CompressionServiceTest, StressTest_ManySmallStrings) {
  for (int i = 0; i < 100; ++i) {
    std::string test_data = generate_random_data(100 + i * 10);
    verify_round_trip(test_data);
  }
}

TEST_F(CompressionServiceTest, StressTest_LargeDataMultipleLevels) {
  std::string large_data = generate_repetitive_data(100000);
  
  for (int level = 1; level <= 22; level += 3) {
    std::vector<char> compressed = CompressionService::compress(large_data, level);
    std::string decompressed = CompressionService::decompress(compressed);
    EXPECT_EQ(large_data, decompressed);
  }
}

// Error handling tests (these should throw exceptions)
TEST_F(CompressionServiceTest, Decompress_EmptyVector) {
  std::vector<char> empty_compressed;
  // Empty vector should return empty string, not throw
  std::string result = CompressionService::decompress(empty_compressed);
  EXPECT_TRUE(result.empty());
}

TEST_F(CompressionServiceTest, Decompress_InvalidData) {
  std::vector<char> invalid_data = {'H', 'e', 'l', 'l', 'o'}; // Not zstd compressed
  EXPECT_THROW(CompressionService::decompress(invalid_data), std::runtime_error);
}

TEST_F(CompressionServiceTest, Decompress_CorruptedData) {
  // First create valid compressed data
  std::string original_data = "Test data for corruption test";
  std::vector<char> valid_compressed = CompressionService::compress(original_data);
  
  // Corrupt the data more severely
  if (valid_compressed.size() > 4) {
    // Corrupt the header bytes which are critical for zstd
    valid_compressed[0] = 0xFF;
    valid_compressed[1] = 0xFF;
    valid_compressed[2] = 0xFF;
    valid_compressed[3] = 0xFF;
    
    EXPECT_THROW(CompressionService::decompress(valid_compressed), std::runtime_error);
  }
}

// Boundary tests
TEST_F(CompressionServiceTest, CompressDecompress_MaximumCompressionLevel) {
  std::string test_data = generate_repetitive_data(10000);
  verify_round_trip(test_data, 22); // Maximum zstd compression level
}

TEST_F(CompressionServiceTest, CompressDecompress_MinimumCompressionLevel) {
  std::string test_data = generate_repetitive_data(10000);
  verify_round_trip(test_data, 1); // Minimum zstd compression level
}

TEST_F(CompressionServiceTest, CompressDecompress_ZeroCompressionLevel) {
  std::string test_data = generate_repetitive_data(10000);
  verify_round_trip(test_data, 0); // Zero compression level (should work)
}

// Real-world data simulation
TEST_F(CompressionServiceTest, CompressDecompress_TextDocument) {
  std::string document = R"(
# Sample Document

This is a sample document that might be processed by the magic folder system.
It contains various types of content including:

- Bullet points
- **Bold text**
- *Italic text*
- `Code snippets`

## Section 1
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor 
incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis 
nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.

## Section 2
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore 
eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt 
in culpa qui officia deserunt mollit anim id est laborum.

### Subsection
More content here with some repetitive patterns that should compress well.
More content here with some repetitive patterns that should compress well.
More content here with some repetitive patterns that should compress well.
)";
  
  verify_round_trip(document);
  
  // Verify good compression for text document
  std::vector<char> compressed = CompressionService::compress(document);
  double compression_ratio = static_cast<double>(compressed.size()) / document.size();
  EXPECT_LT(compression_ratio, 0.6) << "Text document should compress reasonably well";
}

}  // namespace magic_core 