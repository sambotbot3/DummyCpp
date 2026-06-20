#include "dpp/parser/parser.h"

#include "lexer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <vector>

namespace dpp::parser {
namespace {

struct KeywordEntry {
  std::string_view text;
  KeywordKind kind;
};

constexpr std::array<KeywordEntry, 35> kKeywords{{
    {"auto", KeywordKind::auto_kw},
    {"bool", KeywordKind::bool_kw},
    {"break", KeywordKind::break_kw},
    {"case", KeywordKind::case_kw},
    {"catch", KeywordKind::catch_kw},
    {"char", KeywordKind::char_kw},
    {"class", KeywordKind::class_kw},
    {"const", KeywordKind::const_kw},
    {"continue", KeywordKind::continue_kw},
    {"default", KeywordKind::default_kw},
    {"delete", KeywordKind::delete_kw},
    {"do", KeywordKind::do_kw},
    {"double", KeywordKind::double_kw},
    {"else", KeywordKind::else_kw},
    {"float", KeywordKind::float_kw},
    {"for", KeywordKind::for_kw},
    {"if", KeywordKind::if_kw},
    {"int", KeywordKind::int_kw},
    {"long", KeywordKind::long_kw},
    {"namespace", KeywordKind::namespace_kw},
    {"new", KeywordKind::new_kw},
    {"nullptr", KeywordKind::nullptr_kw},
    {"private", KeywordKind::private_kw},
    {"protected", KeywordKind::protected_kw},
    {"public", KeywordKind::public_kw},
    {"return", KeywordKind::return_kw},
    {"static", KeywordKind::static_kw},
    {"struct", KeywordKind::struct_kw},
    {"switch", KeywordKind::switch_kw},
    {"template", KeywordKind::template_kw},
    {"this", KeywordKind::this_kw},
    {"throw", KeywordKind::throw_kw},
    {"try", KeywordKind::try_kw},
    {"typename", KeywordKind::typename_kw},
    {"virtual", KeywordKind::virtual_kw},
}};

std::size_t line_for_offset(const std::string &source, std::size_t offset) {
  std::size_t line = 1;
  for (std::size_t index = 0; index < offset && index < source.size(); ++index) {
    if (source[index] == '\n') {
      ++line;
    }
  }
  return line;
}

std::size_t column_for_offset(const std::string &source, std::size_t offset) {
  std::size_t column = 1;
  for (std::size_t index = offset; index > 0; --index) {
    if (source[index - 1] == '\n') {
      break;
    }
    ++column;
  }
  return column;
}

SourceLocation location_for_offset(const std::string &source, std::size_t offset) {
  return {offset, line_for_offset(source, offset), column_for_offset(source, offset)};
}

std::string trim_copy(std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

std::vector<std::string> split_commas_copy(std::string_view text) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  int depth = 0;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char ch = text[index];
    if (ch == '<' || ch == '(' || ch == '{') {
      ++depth;
    } else if (ch == '>' || ch == ')' || ch == '}') {
      --depth;
    } else if (ch == ',' && depth == 0) {
      parts.push_back(trim_copy(text.substr(start, index - start)));
      start = index + 1;
    }
  }
  parts.push_back(trim_copy(text.substr(start)));
  return parts;
}

std::size_t find_matching(const std::string &source, std::size_t open, char left, char right) {
  int depth = 0;
  for (std::size_t index = open; index < source.size(); ++index) {
    if (source[index] == left) {
      ++depth;
    } else if (source[index] == right) {
      --depth;
      if (depth == 0) {
        return index;
      }
    }
  }
  return std::string::npos;
}

bool parse_template_parameters(std::string_view text, std::vector<std::string> &parameters) {
  parameters.clear();
  for (const std::string &part : split_commas_copy(text)) {
    constexpr std::string_view typename_prefix = "typename ";
    constexpr std::string_view class_prefix = "class ";
    std::string_view view(part);
    if (view.rfind(typename_prefix, 0) == 0) {
      parameters.push_back(trim_copy(view.substr(typename_prefix.size())));
    } else if (view.rfind(class_prefix, 0) == 0) {
      parameters.push_back(trim_copy(view.substr(class_prefix.size())));
    } else {
      return false;
    }
    if (parameters.back().empty()) {
      return false;
    }
  }
  return !parameters.empty();
}

bool parse_function_signature(std::string_view text, FunctionDecl &function) {
  const std::size_t open = text.find('(');
  const std::size_t close = text.rfind(')');
  if (open == std::string_view::npos || close == std::string_view::npos || close < open) {
    return false;
  }

  const std::string before = trim_copy(text.substr(0, open));
  const std::size_t name_begin = before.find_last_of(" \t\r\n*&");
  if (name_begin == std::string::npos) {
    return false;
  }

  function.return_type = trim_copy(std::string_view(before).substr(0, name_begin + 1));
  function.name = trim_copy(std::string_view(before).substr(name_begin + 1));
  if (function.return_type.empty() || function.name.empty()) {
    return false;
  }

  function.parameters.clear();
  const std::string parameter_text = trim_copy(text.substr(open + 1, close - open - 1));
  if (parameter_text.empty() || parameter_text == "void") {
    return true;
  }

  for (const std::string &part : split_commas_copy(parameter_text)) {
    const std::size_t param_name_begin = part.find_last_of(" \t\r\n*&");
    if (param_name_begin == std::string::npos) {
      return false;
    }
    FunctionParameter parameter;
    parameter.type = trim_copy(std::string_view(part).substr(0, param_name_begin + 1));
    parameter.name = trim_copy(std::string_view(part).substr(param_name_begin + 1));
    if (parameter.type.empty() || parameter.name.empty()) {
      return false;
    }
    function.parameters.push_back(parameter);
  }
  return true;
}

std::vector<FunctionTemplateDecl> parse_function_templates(const std::string &source) {
  std::vector<FunctionTemplateDecl> templates;
  std::size_t search = 0;
  constexpr std::string_view marker = "template";
  while ((search = source.find(marker, search)) != std::string::npos) {
    const bool before_ok =
        search == 0 || !(std::isalnum(static_cast<unsigned char>(source[search - 1])) ||
                         source[search - 1] == '_');
    const std::size_t after_marker = search + marker.size();
    const bool after_ok =
        after_marker >= source.size() ||
        !(std::isalnum(static_cast<unsigned char>(source[after_marker])) ||
          source[after_marker] == '_');
    if (!before_ok || !after_ok) {
      search = after_marker;
      continue;
    }

    const std::size_t angle_open = source.find('<', after_marker);
    if (angle_open == std::string::npos) {
      break;
    }
    const std::size_t angle_close = find_matching(source, angle_open, '<', '>');
    if (angle_close == std::string::npos) {
      break;
    }

    std::vector<std::string> parameters;
    if (!parse_template_parameters(
            std::string_view(source).substr(angle_open + 1, angle_close - angle_open - 1),
            parameters)) {
      search = angle_close + 1;
      continue;
    }

    const std::size_t body_open = source.find('{', angle_close + 1);
    if (body_open == std::string::npos) {
      break;
    }
    const std::size_t body_close = find_matching(source, body_open, '{', '}');
    if (body_close == std::string::npos) {
      break;
    }

    FunctionDecl function;
    if (parse_function_signature(
            std::string_view(source).substr(angle_close + 1, body_open - angle_close - 1),
            function)) {
      function.body = source.substr(body_open + 1, body_close - body_open - 1);
      function.range = {location_for_offset(source, angle_close + 1),
                        location_for_offset(source, body_close + 1)};
      FunctionTemplateDecl decl;
      decl.parameters = parameters;
      decl.function = function;
      decl.range = {location_for_offset(source, search), location_for_offset(source, body_close + 1)};
      templates.push_back(decl);
    }
    search = body_close + 1;
  }
  return templates;
}

} // namespace

KeywordKind classify_keyword(const std::string &text) {
  const auto found = std::lower_bound(
      kKeywords.begin(), kKeywords.end(), text,
      [](const KeywordEntry &entry, const std::string &value) {
        return entry.text.compare(std::string_view(value)) < 0;
      });
  if (found != kKeywords.end() && found->text == text) {
    return found->kind;
  }
  return KeywordKind::none;
}

ParsedSource parse_translation_unit(const std::string &source) {
  ParsedSource parsed;
  parsed.text = source;
  parsed.tokens = lex_translation_unit(source);
  parsed.function_templates = parse_function_templates(source);
  return parsed;
}

} // namespace dpp::parser
