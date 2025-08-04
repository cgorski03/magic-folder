#pragma once

#include <vector>
#include <string_view>

namespace magic_core {

class CompressionService {
    public:
        /**
         * @brief Compresses a block of data using Zstandard.
         * @param data The data to compress.
         * @param compression_level The zstd compression level (default is 3).
         * @return A vector of chars containing the compressed binary data.
         */
        static std::vector<char> compress(std::string_view data, int compression_level = 3);
    
        /**
         * @brief Decompresses a block of Zstandard-compressed data.
         * @param compressed_data The binary data to decompress.
         * @return A string containing the original, decompressed data.
         */
        static std::string decompress(const std::vector<char>& compressed_data);
};
}