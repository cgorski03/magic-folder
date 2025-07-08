#pragma once

#include <string>
#include <vector>
#include <map>

struct Chunk {
    std::string content;
    int chunk_index;
    std::map<std::string, std::string> metadata;
};