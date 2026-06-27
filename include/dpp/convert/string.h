#pragma once

#include "dpp/parser/parser.h"

#include <string>

namespace dpp::convert {

struct StringResult {
  std::string source;
  bool used_string = false;
};

StringResult lower_strings(const std::string &source,
                           const parser::ParsedSource &parsed);

} // namespace dpp::convert
