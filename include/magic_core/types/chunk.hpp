#pragma once

#include <string>
#include <vector>

struct Chunk {
    std::string content;
    int chunk_index;
    std::vector<float> vector_embedding;
};