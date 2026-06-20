#pragma once

#include "dpp/parser/parser.h"

#include <string>

namespace dpp::convert {

struct TemplateResult {
  std::string source;
};

TemplateResult lower_function_templates(const parser::ParsedSource &source);

} // namespace dpp::convert
