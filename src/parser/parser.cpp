#include "dpp/parser/parser.h"

#include <algorithm>
#include <array>
#include <cctype>
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

bool is_identifier_start(char ch) {
  return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool is_identifier_continue(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

bool is_operator_char(char ch) {
  switch (ch) {
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
  case '=':
  case '!':
  case '<':
  case '>':
  case '&':
  case '|':
  case '^':
  case '~':
  case ':':
  case '?':
    return true;
  default:
    return false;
  }
}

bool is_punctuation_char(char ch) {
  switch (ch) {
  case '(':
  case ')':
  case '{':
  case '}':
  case '[':
  case ']':
  case ';':
  case ',':
  case '.':
    return true;
  default:
    return false;
  }
}

class Lexer {
public:
  explicit Lexer(const std::string &source) : source_(source) {}

  std::vector<Token> lex() {
    std::vector<Token> tokens;
    while (!at_end()) {
      const char ch = peek();
      if (std::isspace(static_cast<unsigned char>(ch))) {
        advance();
      } else if (ch == '#') {
        tokens.push_back(read_preprocessor());
      } else if (is_identifier_start(ch)) {
        tokens.push_back(read_identifier_or_keyword());
      } else if (std::isdigit(static_cast<unsigned char>(ch))) {
        tokens.push_back(read_number());
      } else if (ch == '"') {
        tokens.push_back(read_quoted(TokenKind::string_literal, '"'));
      } else if (ch == '\'') {
        tokens.push_back(read_quoted(TokenKind::char_literal, '\''));
      } else if (ch == '/' && peek_next() == '/') {
        tokens.push_back(read_line_comment());
      } else if (ch == '/' && peek_next() == '*') {
        tokens.push_back(read_block_comment());
      } else if (is_operator_char(ch)) {
        tokens.push_back(read_operator());
      } else if (is_punctuation_char(ch)) {
        tokens.push_back(read_single(TokenKind::punctuation));
      } else {
        tokens.push_back(read_single(TokenKind::unknown));
      }
    }

    Token eof;
    eof.kind = TokenKind::end_of_file;
    eof.range.begin = location();
    eof.range.end = location();
    tokens.push_back(eof);
    return tokens;
  }

private:
  bool at_end() const { return index_ >= source_.size(); }
  char peek() const { return at_end() ? '\0' : source_[index_]; }
  char peek_next() const { return index_ + 1 < source_.size() ? source_[index_ + 1] : '\0'; }

  SourceLocation location() const {
    return SourceLocation{index_, line_, column_};
  }

  char advance() {
    const char ch = source_[index_++];
    if (ch == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }
    return ch;
  }

  Token finish(TokenKind kind, SourceLocation begin, std::size_t begin_index) {
    Token token;
    token.kind = kind;
    token.text = source_.substr(begin_index, index_ - begin_index);
    token.range.begin = begin;
    token.range.end = location();
    if (kind == TokenKind::identifier) {
      token.keyword = classify_keyword(token.text);
      if (token.keyword != KeywordKind::none) {
        token.kind = TokenKind::keyword;
      }
    }
    return token;
  }

  Token read_single(TokenKind kind) {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    advance();
    return finish(kind, begin, begin_index);
  }

  Token read_identifier_or_keyword() {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    while (!at_end() && is_identifier_continue(peek())) {
      advance();
    }
    return finish(TokenKind::identifier, begin, begin_index);
  }

  Token read_number() {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '.')) {
      advance();
    }
    return finish(TokenKind::number_literal, begin, begin_index);
  }

  Token read_quoted(TokenKind kind, char quote) {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    advance();
    while (!at_end()) {
      const char ch = advance();
      if (ch == '\\' && !at_end()) {
        advance();
      } else if (ch == quote) {
        break;
      }
    }
    return finish(kind, begin, begin_index);
  }

  Token read_preprocessor() {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    while (!at_end() && peek() != '\n') {
      advance();
    }
    return finish(TokenKind::preprocessor, begin, begin_index);
  }

  Token read_line_comment() {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    while (!at_end() && peek() != '\n') {
      advance();
    }
    return finish(TokenKind::comment, begin, begin_index);
  }

  Token read_block_comment() {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    advance();
    advance();
    while (!at_end()) {
      if (peek() == '*' && peek_next() == '/') {
        advance();
        advance();
        break;
      }
      advance();
    }
    return finish(TokenKind::comment, begin, begin_index);
  }

  Token read_operator() {
    const SourceLocation begin = location();
    const std::size_t begin_index = index_;
    while (!at_end() && is_operator_char(peek())) {
      advance();
    }
    return finish(TokenKind::oper, begin, begin_index);
  }

  const std::string &source_;
  std::size_t index_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
};

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
  parsed.tokens = Lexer(source).lex();
  return parsed;
}

} // namespace dpp::parser
