#pragma once

#include "dpp/parser/parser.h"

#include <string>
#include <vector>

namespace dpp::parser {

enum class DiagnosticLevel {
  warning,
  error,
};

struct Diagnostic {
  DiagnosticLevel level;
  std::string message;
};

std::vector<Diagnostic> check_bootstrap_syntax(const ParsedSource &source);
bool has_errors(const std::vector<Diagnostic> &diagnostics);

} // namespace dpp::parser

