#include "magic_core/services/compression_service.hpp"
#include <zstd.h>
#include <stdexcept>

namespace magic_core {

std::vector<char> CompressionService::compress(std::string_view data, int compression_level) {
    if (data.empty()) {
        return {};
    }
    size_t const worst_case_size = ZSTD_compressBound(data.size());

    std::vector<char> compressed_buffer(worst_case_size);

    size_t const compressed_size = ZSTD_compress(
        compressed_buffer.data(),
        compressed_buffer.size(),
        data.data(),
        data.size(),
        compression_level
    );

    if (ZSTD_isError(compressed_size)) {
        throw std::runtime_error("ZSTD compression failed: " + std::string(ZSTD_getErrorName(compressed_size)));
    }

    compressed_buffer.resize(compressed_size);

    return compressed_buffer;
}

std::string CompressionService::decompress(const std::vector<char>& compressed_data) {
    if (compressed_data.empty()) {
        return "";
    }

    unsigned long long const decompressed_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());

    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error("Failed to get decompressed size or data is not zstd format.");
    }

    std::string decompressed_buffer(decompressed_size, '\0');

    size_t const actual_dSize = ZSTD_decompress(
        decompressed_buffer.data(),
        decompressed_buffer.size(),
        compressed_data.data(),
        compressed_data.size()
    );

    if (ZSTD_isError(actual_dSize) || actual_dSize != decompressed_size) {
        throw std::runtime_error("ZSTD decompression failed: " + std::string(ZSTD_getErrorName(actual_dSize)));
    }

    return decompressed_buffer;
}

}