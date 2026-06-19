#pragma once

#include <string>

namespace dpp::convert {

struct MemoryResult {
  std::string source;
  bool used_memory = false;
};

MemoryResult lower_memory(const std::string &source);

} // namespace dpp::convert
