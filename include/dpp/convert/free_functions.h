#pragma once

#include <string>

namespace dpp::convert {

struct FreeFunctionResult {
  std::string source;
};

FreeFunctionResult lower_free_functions(const std::string &source);

} // namespace dpp::convert
