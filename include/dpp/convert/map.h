#pragma once

#include <string>

namespace dpp::convert {

struct MapResult {
  std::string source;
  bool used_map = false;
};

MapResult lower_maps(const std::string &source);

} // namespace dpp::convert
