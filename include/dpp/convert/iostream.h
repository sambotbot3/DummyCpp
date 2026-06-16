#pragma once

#include <string>

namespace dpp::convert {

struct IostreamResult {
  std::string source;
  bool used_stdio = false;
};

IostreamResult lower_iostreams(const std::string &source);

} // namespace dpp::convert

