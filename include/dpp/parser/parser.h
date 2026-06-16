#pragma once

#include <string>

namespace dpp::parser {

struct ParsedSource {
  std::string text;
};

ParsedSource parse_translation_unit(const std::string &source);

} // namespace dpp::parser

