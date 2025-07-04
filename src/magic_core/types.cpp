#include "magic_core/types.hpp"

namespace magic_core {

std::string to_string(FileType type) {
  switch (type) {
    case FileType::Text:
      return "Text";
    case FileType::PDF:
      return "PDF";
    case FileType::Markdown:
      return "Markdown";
    case FileType::Code:
      return "Code";
    default:
      return "Unknown";
  }
}

FileType file_type_from_string(const std::string& str) {
  if (str == "Text")
    return FileType::Text;
  if (str == "PDF")
    return FileType::PDF;
  if (str == "Markdown")
    return FileType::Markdown;
  if (str == "Code")
    return FileType::Code;
  return FileType::Unknown;
}

}  // namespace magic_core