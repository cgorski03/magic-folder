#pragma once

#include <string>
#include <vector>

namespace magic_core {

struct Chunk {
    std::string content;
    int chunk_index;
    std::vector<float> vector_embedding;
};

}  // namespace magic_core