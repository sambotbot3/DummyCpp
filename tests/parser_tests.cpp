#include "dpp/parser/parser.h"
#include "dpp/parser/syntax_checker.h"

#include <cassert>
#include <string>
#include <vector>

namespace {

bool has_error_containing(const std::vector<dpp::parser::Diagnostic> &diagnostics,
                          const std::string &needle) {
  for (const dpp::parser::Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.level == dpp::parser::DiagnosticLevel::error &&
        diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void test_keywords() {
  using dpp::parser::KeywordKind;
  assert(dpp::parser::classify_keyword("class") == KeywordKind::class_kw);
  assert(dpp::parser::classify_keyword("template") == KeywordKind::template_kw);
  assert(dpp::parser::classify_keyword("virtual") == KeywordKind::virtual_kw);
  assert(dpp::parser::classify_keyword("identifier") == KeywordKind::none);
}

void test_tokens_and_locations() {
  const dpp::parser::ParsedSource parsed =
      dpp::parser::parse_translation_unit("#include <iostream>\nint main() {\n  return 0;\n}\n");

  bool saw_include = false;
  bool saw_return = false;
  for (const dpp::parser::Token &token : parsed.tokens) {
    if (token.kind == dpp::parser::TokenKind::preprocessor &&
        token.text == "#include <iostream>") {
      saw_include = true;
      assert(token.range.begin.line == 1);
      assert(token.range.begin.column == 1);
    }
    if (token.kind == dpp::parser::TokenKind::keyword &&
        token.keyword == dpp::parser::KeywordKind::return_kw) {
      saw_return = true;
      assert(token.range.begin.line == 3);
      assert(token.range.begin.column == 3);
    }
  }
  assert(saw_include);
  assert(saw_return);
}

void test_function_template_parse() {
  const dpp::parser::ParsedSource parsed = dpp::parser::parse_translation_unit(
      "template <typename T>\n"
      "T identity(T value) {\n"
      "  return value;\n"
      "}\n");

  assert(parsed.function_templates.size() == 1);
  const dpp::parser::FunctionTemplateDecl &decl = parsed.function_templates.front();
  assert(decl.parameters.size() == 1);
  assert(decl.parameters.front() == "T");
  assert(decl.function.return_type == "T");
  assert(decl.function.name == "identity");
  assert(decl.function.parameters.size() == 1);
  assert(decl.function.parameters.front().type == "T");
  assert(decl.function.parameters.front().name == "value");
}

void test_unsupported_diagnostics() {
  const dpp::parser::ParsedSource parsed = dpp::parser::parse_translation_unit(
      "#include <thread>\n"
      "class Base { public: virtual int value() const { return 1; } };\n"
      "void f() { throw 1; }\n");
  const std::vector<dpp::parser::Diagnostic> diagnostics =
      dpp::parser::check_bootstrap_syntax(parsed);

  assert(has_error_containing(diagnostics, "unsupported include"));
  assert(has_error_containing(diagnostics, "virtual methods"));
  assert(has_error_containing(diagnostics, "exceptions"));
  assert(dpp::parser::has_errors(diagnostics));
}

} // namespace

int main() {
  test_keywords();
  test_tokens_and_locations();
  test_function_template_parse();
  test_unsupported_diagnostics();
  return 0;
}
