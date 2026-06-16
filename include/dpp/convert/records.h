#pragma once

#include <string>

namespace dpp::convert {

struct RecordsResult {
  std::string source;
  bool lowered_records = false;
};

RecordsResult lower_records(const std::string &source);

} // namespace dpp::convert
