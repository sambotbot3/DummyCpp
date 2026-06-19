#pragma once

#include <string>

namespace dpp::convert {

struct AlgorithmResult {
  std::string source;
  bool used_algorithm = false;
};

AlgorithmResult lower_algorithms(const std::string &source);

} // namespace dpp::convert
