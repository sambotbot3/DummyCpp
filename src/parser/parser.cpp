#include "dpp/parser/parser.h"

#include "lexer.h"

#include <algorithm>
#include <array>
#include <string_view>

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
  return parsed;
}

} // namespace dpp::parser
