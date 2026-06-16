#pragma once

#include <string>

namespace dpp::convert {

struct VectorResult {
  std::string source;
  bool used_vector = false;
};

VectorResult lower_vectors(const std::string &source);

} // namespace dpp::convert
