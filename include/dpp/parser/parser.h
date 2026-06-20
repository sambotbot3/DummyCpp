#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dpp::parser {

struct SourceLocation {
  std::size_t offset = 0;
  std::size_t line = 1;
  std::size_t column = 1;
};

struct SourceRange {
  SourceLocation begin;
  SourceLocation end;
};

enum class TokenKind {
  identifier,
  keyword,
  number_literal,
  string_literal,
  char_literal,
  oper,
  punctuation,
  preprocessor,
  comment,
  end_of_file,
  unknown,
};

enum class KeywordKind {
  none,
  include,
  using_kw,
  namespace_kw,
  struct_kw,
  class_kw,
  public_kw,
  private_kw,
  protected_kw,
  const_kw,
  static_kw,
  void_kw,
  bool_kw,
  char_kw,
  int_kw,
  long_kw,
  float_kw,
  double_kw,
  auto_kw,
  return_kw,
  if_kw,
  else_kw,
  for_kw,
  while_kw,
  do_kw,
  break_kw,
  continue_kw,
  switch_kw,
  case_kw,
  default_kw,
  new_kw,
  delete_kw,
  this_kw,
  nullptr_kw,
  template_kw,
  typename_kw,
  virtual_kw,
  try_kw,
  catch_kw,
  throw_kw,
};

struct Token {
  TokenKind kind = TokenKind::unknown;
  KeywordKind keyword = KeywordKind::none;
  std::string text;
  SourceRange range;
};

struct FunctionParameter {
  std::string type;
  std::string name;
};

struct FunctionDecl {
  std::string return_type;
  std::string name;
  std::vector<FunctionParameter> parameters;
  std::string body;
  SourceRange range;
};

struct FunctionTemplateDecl {
  std::vector<std::string> parameters;
  FunctionDecl function;
  SourceRange range;
};

struct ParsedSource {
  std::string text;
  std::vector<Token> tokens;
  std::vector<FunctionTemplateDecl> function_templates;
};

ParsedSource parse_translation_unit(const std::string &source);
KeywordKind classify_keyword(const std::string &text);

} // namespace dpp::parser
