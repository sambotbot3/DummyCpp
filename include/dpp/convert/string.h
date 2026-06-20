#pragma once

#include <string>

namespace dpp::convert {

struct StringResult {
  std::string source;
  bool used_string = false;
};

StringResult lower_strings(const std::string &source);

} // namespace dpp::convert
