#include "dpp/parser/syntax_checker.h"

#include <regex>

namespace dpp::parser {
namespace {

void add_error(std::vector<Diagnostic> &diagnostics, const std::string &message) {
  diagnostics.push_back({DiagnosticLevel::error, message});
}

bool is_supported_include(const std::string &include) {
  return include == "#include <algorithm>" || include == "#include <iostream>" ||
         include == "#include <vector>" || include == "#include <cassert>" ||
         include == "#include <assert.h>" ||
         include == "#include <string>" ||
         include == "#include <memory>" || include == "#include <map>" ||
         include == "#include <unordered_map>";
}

} // namespace

std::vector<Diagnostic> check_bootstrap_syntax(const ParsedSource &source) {
  std::vector<Diagnostic> diagnostics;

  if (source.text.find("std::cout") != std::string::npos &&
      source.text.find("#include <iostream>") == std::string::npos) {
    diagnostics.push_back(
        {DiagnosticLevel::warning, "std::cout used without #include <iostream>; continuing"});
  }

  for (const Token &token : source.tokens) {
    if (token.kind == TokenKind::preprocessor && token.text.rfind("#include", 0) == 0 &&
        !is_supported_include(token.text)) {
      add_error(diagnostics, "unsupported include in bootstrap subset: " + token.text);
    }

    if (token.kind != TokenKind::keyword) {
      continue;
    }
    switch (token.keyword) {
    case KeywordKind::try_kw:
    case KeywordKind::catch_kw:
    case KeywordKind::throw_kw:
      add_error(diagnostics, "exceptions are not supported yet");
      break;
    case KeywordKind::virtual_kw:
      add_error(diagnostics, "virtual methods are not supported yet");
      break;
    case KeywordKind::template_kw:
      add_error(diagnostics, "user-defined templates are not supported yet");
      break;
    default:
      break;
    }
  }

  const std::regex inheritance_re(R"(\b(class|struct)\s+[A-Za-z_]\w*\s*:\s*([^{]+)\{)");
  for (std::sregex_iterator it(source.text.begin(), source.text.end(), inheritance_re), end;
       it != end; ++it) {
    const std::string clause = (*it)[2].str();
    if (std::regex_search(clause, std::regex(R"(\bvirtual\b)"))) {
      add_error(diagnostics, "virtual inheritance is not supported yet");
    }
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
