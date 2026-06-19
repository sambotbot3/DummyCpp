#include "lexer.h"

#include <cstddef>

namespace dpp::parser {
namespace {

struct Cursor {
  explicit Cursor(const std::string &input) : source(input) {}

  const std::string &source;
  const char *cursor = source.data();
  const char *marker = nullptr;
  const char *limit = source.data() + source.size();
  std::size_t line = 1;
  std::size_t column = 1;
};

SourceLocation location(const Cursor &ctx, const char *cursor) {
  return SourceLocation{
      static_cast<std::size_t>(cursor - ctx.source.data()),
      ctx.line,
      ctx.column,
  };
}

void advance_location(Cursor &ctx, const char *begin, const char *end) {
  for (const char *it = begin; it != end; ++it) {
    if (*it == '\n') {
      ++ctx.line;
      ctx.column = 1;
    } else {
      ++ctx.column;
    }
  }
}

Token make_token(Cursor &ctx, TokenKind kind, const char *begin, const SourceLocation &begin_location) {
  advance_location(ctx, begin, ctx.cursor);

  Token token;
  token.kind = kind;
  token.text.assign(begin, ctx.cursor);
  token.range.begin = begin_location;
  token.range.end = location(ctx, ctx.cursor);
  if (kind == TokenKind::identifier) {
    token.keyword = classify_keyword(token.text);
    if (token.keyword != KeywordKind::none) {
      token.kind = TokenKind::keyword;
    }
  }
  return token;
}

} // namespace

std::vector<Token> lex_translation_unit(const std::string &source) {
  Cursor ctx(source);
  std::vector<Token> tokens;

  while (ctx.cursor < ctx.limit) {
    const char *token_begin = ctx.cursor;
    const SourceLocation token_location = location(ctx, token_begin);

#define YYPEEK ((ctx.cursor < ctx.limit) ? *ctx.cursor : '\0')
#define YYSKIP do { ++ctx.cursor; } while (false);
#define YYBACKUP do { ctx.marker = ctx.cursor; } while (false);
#define YYRESTORE do { ctx.cursor = ctx.marker; } while (false);

    /*!re2c
      re2c:api:style = free-form;
      re2c:define:YYCTYPE = char;
      re2c:define:YYCURSOR = ctx.cursor;
      re2c:define:YYMARKER = ctx.marker;
      re2c:define:YYLIMIT = ctx.limit;
      re2c:yyfill:enable = 0;

      ws = [\x09\x0a\x0b\x0c\x0d\x20]+;
      ident = [A-Za-z_][A-Za-z_0-9]*;
      number = [0-9][A-Za-z_0-9.]*;
      string = "\"" ([^"\x00\\] | "\\" [^\x00])* "\""?;
      character = "'" ([^'\x00\\] | "\\" [^\x00])* "'"?;
      preproc = "#" [^\x00\n]*;
      line_comment = "//" [^\x00\n]*;
      block_comment = "/*" ([^*\x00] | "*" [^/\x00])* ("*/")?;
      oper = [+\-*/%=!<>&|^~:?]+;
      punctuation = [(){}\[\];,.];

      ws {
        advance_location(ctx, token_begin, ctx.cursor);
        continue;
      }
      preproc { tokens.push_back(make_token(ctx, TokenKind::preprocessor, token_begin, token_location)); continue; }
      ident { tokens.push_back(make_token(ctx, TokenKind::identifier, token_begin, token_location)); continue; }
      number { tokens.push_back(make_token(ctx, TokenKind::number_literal, token_begin, token_location)); continue; }
      string { tokens.push_back(make_token(ctx, TokenKind::string_literal, token_begin, token_location)); continue; }
      character { tokens.push_back(make_token(ctx, TokenKind::char_literal, token_begin, token_location)); continue; }
      line_comment { tokens.push_back(make_token(ctx, TokenKind::comment, token_begin, token_location)); continue; }
      block_comment { tokens.push_back(make_token(ctx, TokenKind::comment, token_begin, token_location)); continue; }
      oper { tokens.push_back(make_token(ctx, TokenKind::oper, token_begin, token_location)); continue; }
      punctuation { tokens.push_back(make_token(ctx, TokenKind::punctuation, token_begin, token_location)); continue; }
      * { tokens.push_back(make_token(ctx, TokenKind::unknown, token_begin, token_location)); continue; }
    */

#undef YYSKIP
#undef YYBACKUP
#undef YYRESTORE
#undef YYPEEK
  }

  Token eof;
  eof.kind = TokenKind::end_of_file;
  eof.range.begin = location(ctx, ctx.cursor);
  eof.range.end = eof.range.begin;
  tokens.push_back(eof);
  return tokens;
}

} // namespace dpp::parser
