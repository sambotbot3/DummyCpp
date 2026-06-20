#pragma once

#include <string>

namespace dpp::convert {

struct AssertionsResult {
  std::string source;
  bool used_assert = false;
};

AssertionsResult lower_assertions(const std::string &source);

} // namespace dpp::convert
