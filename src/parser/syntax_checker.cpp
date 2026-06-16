#include "dpp/parser/syntax_checker.h"

namespace dpp::parser {

std::vector<Diagnostic> check_bootstrap_syntax(const ParsedSource &source) {
  std::vector<Diagnostic> diagnostics;

  if (source.text.find("std::cout") != std::string::npos &&
      source.text.find("#include <iostream>") == std::string::npos) {
    diagnostics.push_back(
        {DiagnosticLevel::warning, "std::cout used without #include <iostream>; continuing"});
  }

  return diagnostics;
}

bool has_errors(const std::vector<Diagnostic> &diagnostics) {
  for (const Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.level == DiagnosticLevel::error) {
      return true;
    }
  }
  return false;
}

} // namespace dpp::parser

