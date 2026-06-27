#include "dpp/parser/syntax_checker.h"

#include <regex>

namespace dpp::parser {
namespace {

void add_error(std::vector<Diagnostic> &diagnostics, const std::string &message) {
  diagnostics.push_back({DiagnosticLevel::error, message});
}

bool is_supported_include(const std::string &include) {
  static const char *const allowed[] = {
      "#include <algorithm>",   "#include <iostream>",
      "#include <vector>",      "#include <cassert>",
      "#include <assert.h>",    "#include <string>",
      "#include <memory>",      "#include <map>",
      "#include <unordered_map>",
      // C compat headers — stripped by lower_cpp_surface_types; no C equivalent needed
      "#include <cstddef>",     "#include <cstdlib>",
      "#include <cstring>",     "#include <cmath>",
      "#include <climits>",     "#include <utility>",
      "#include <optional>",
      "#include <type_traits>", "#include <functional>",
      "#include <numeric>",     "#include <stdexcept>",
      nullptr};
  for (int i = 0; allowed[i]; ++i) {
    if (include == allowed[i]) return true;
  }
  return false;
}

} // namespace

std::vector<Diagnostic> check_bootstrap_syntax(const ParsedSource &source) {
  std::vector<Diagnostic> diagnostics;
  std::size_t template_keyword_count = 0;

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
      ++template_keyword_count;
      break;
    default:
      break;
    }
  }

  if (template_keyword_count != source.function_templates.size()) {
    add_error(diagnostics,
              "unsupported template declaration; only simple function templates are supported");
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
