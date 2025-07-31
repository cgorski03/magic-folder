#pragma once

#include <string>

namespace magic_core {

// Core file type enumeration
enum class FileType { Text, PDF, Markdown, Code, Unknown };

// Conversion utilities
std::string to_string(FileType type);
FileType file_type_from_string(const std::string& str);

}  // namespace magic_core