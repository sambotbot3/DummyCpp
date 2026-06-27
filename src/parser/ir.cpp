#include "dpp/parser/ir.h"
#include "dpp/string_utils.h"

#include <cctype>
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace dpp::ir {
namespace {

bool is_ident_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Returns the index just past the closing brace matching the '{' at open_pos.
std::size_t matching_close_brace(const std::string &src, std::size_t open_pos) {
  int depth = 0;
  bool in_string = false;
  bool in_char = false;
  for (std::size_t i = open_pos; i < src.size(); ++i) {
    const char c = src[i];
    if (in_string) {
      if (c == '\\') { ++i; }
      else if (c == '"') { in_string = false; }
      continue;
    }
    if (in_char) {
      if (c == '\\') { ++i; }
      else if (c == '\'') { in_char = false; }
      continue;
    }
    if (c == '"') { in_string = true; continue; }
    if (c == '\'') { in_char = true; continue; }
    if (c == '{') { ++depth; }
    else if (c == '}') {
      --depth;
      if (depth == 0) return i + 1;
    }
  }
  return std::string::npos;
}

// Extract field/method name from a declaration line inside a struct body.
// Returns the bare identifier name, or empty on failure.
std::string extract_member_name(const std::string &decl) {
  std::string s = trim(decl);
  // Strip trailing ;
  while (!s.empty() && s.back() == ';') { s.pop_back(); s = trim(s); }
  // Strip trailing inline body block  "{ ... }"
  if (!s.empty() && s.back() == '}') {
    const std::size_t open = s.rfind('{');
    if (open != std::string::npos) s = trim(s.substr(0, open));
  }
  // Strip trailing "const" method qualifier (e.g. "int sum() const")
  if (s.size() > 5 && s.compare(s.size() - 5, 5, "const") == 0) {
    const char before = s.size() > 5 ? s[s.size() - 6] : ' ';
    if (!is_ident_char(before)) s = trim(s.substr(0, s.size() - 5));
  }
  // For methods (has '('), the name is the identifier before the first '('
  const std::size_t paren = s.find('(');
  const std::string before_paren = (paren != std::string::npos) ? trim(s.substr(0, paren)) : s;
  // Walk backwards to find the last identifier token
  std::size_t end = before_paren.size();
  while (end > 0 && is_ident_char(before_paren[end - 1])) --end;
  if (end == before_paren.size()) return "";
  return before_paren.substr(end);
}

// Parse a record (struct/class) body text and populate record fields.
void parse_record_body(const std::string &body, DppRecord &record) {
  // Only look at the first brace level; nested braces are method bodies.
  int depth = 0;
  std::ostringstream current_line;
  for (std::size_t i = 0; i < body.size(); ++i) {
    const char c = body[i];
    if (c == '{') { ++depth; if (depth > 1) current_line << c; continue; }
    if (c == '}') {
      --depth;
      if (depth >= 1) current_line << c;
      continue;
    }
    if (depth > 1) { current_line << c; continue; }
    if (c == '\n' || c == ';') {
      const std::string line = trim(current_line.str());
      current_line.str("");
      current_line.clear();
      if (line.empty()) continue;
      if (line == "public:" || line == "private:" || line == "protected:") continue;
      const bool is_method = line.find('(') != std::string::npos;
      const std::string name = extract_member_name(line);
      if (name.empty() || name == record.name) continue; // skip constructor
      if (is_method) {
        record.method_names.push_back(name);
      } else {
        // Extract type: everything before the name
        const std::size_t name_pos = line.rfind(name);
        if (name_pos == std::string::npos) continue;
        const std::string type = trim(line.substr(0, name_pos));
        if (!type.empty()) {
          record.fields.push_back({type, name});
        }
      }
    } else {
      current_line << c;
    }
  }
}

// Parse base class names from a ": public Base, protected Other" clause.
std::vector<std::string> parse_bases(const std::string &clause) {
  std::vector<std::string> bases;
  static const std::regex base_re(R"(\b(?:public|protected|private)\s+([A-Za-z_]\w*))");
  for (std::sregex_iterator it(clause.begin(), clause.end(), base_re), end; it != end; ++it) {
    bases.push_back((*it)[1].str());
  }
  return bases;
}

// Parse a parameter list string into Param structs.
std::vector<Param> parse_params(const std::string &params_text) {
  std::vector<Param> params;
  if (trim(params_text).empty() || trim(params_text) == "void") return params;
  for (const std::string &part : split_commas(params_text)) {
    const std::string p = trim(part);
    if (p.empty()) continue;
    std::size_t end = p.size();
    while (end > 0 && is_ident_char(p[end - 1])) --end;
    if (end == 0 || end == p.size()) {
      params.push_back({p, ""});
      continue;
    }
    if (!std::isalpha(static_cast<unsigned char>(p[end])) && p[end] != '_') {
      params.push_back({p, ""});
      continue;
    }
    params.push_back({trim(p.substr(0, end)), p.substr(end)});
  }
  return params;
}

std::vector<DppRecord> extract_records(const std::string &source) {
  std::vector<DppRecord> records;
  static const std::regex record_re(
      R"(\b(struct|class)\s+([A-Za-z_]\w*)([^{;]*)\{)");
  std::sregex_iterator it(source.begin(), source.end(), record_re);
  const std::sregex_iterator end;
  for (; it != end; ++it) {
    const std::smatch &m = *it;
    DppRecord record;
    record.name = m[2].str();
    const std::string between = trim(m[3].str());
    if (!between.empty() && between[0] == ':') {
      record.bases = parse_bases(between.substr(1));
    }
    // Find the brace block
    const std::size_t brace_open = m.position() + m.length() - 1;
    const std::size_t brace_close = matching_close_brace(source, brace_open);
    if (brace_close == std::string::npos) continue;
    parse_record_body(source.substr(brace_open, brace_close - brace_open), record);
    records.push_back(std::move(record));
  }
  return records;
}

std::vector<DppFunction> extract_functions(const std::string &source,
                                           const parser::ParsedSource &parsed) {
  std::vector<DppFunction> functions;
  // Find top-level function definitions by scanning for lines matching
  // "ReturnType name(params) {" at column 0.
  static const std::regex func_re(
      R"(^([A-Za-z_][A-Za-z0-9_ :*&<>]*?)\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:const\s*)?\{)");

  for (const std::string &line : split_lines(source)) {
    if (line.empty() || line[0] == ' ' || line[0] == '\t' || line[0] == '/' || line[0] == '#')
      continue;
    if (starts_with(line, "static") || starts_with(line, "inline") ||
        starts_with(line, "struct") || starts_with(line, "class") || starts_with(line, "template"))
      continue;
    std::smatch m;
    if (!std::regex_search(line, m, func_re)) continue;
    const std::string name = m[2].str();
    if (name == "main") continue;
    DppFunction fn;
    fn.name = name;
    fn.return_type = trim(m[1].str());
    fn.params = parse_params(m[3].str());

    // Locate body token range in parsed.tokens via name match
    // This is a best-effort approximation for the skeleton.
    fn.body_token_begin = 0;
    fn.body_token_end = 0;
    for (std::size_t i = 0; i < parsed.tokens.size(); ++i) {
      const parser::Token &tok = parsed.tokens[i];
      if (tok.kind == parser::TokenKind::identifier && tok.text == name) {
        fn.body_token_begin = i;
        // Scan forward to the matching closing brace
        int depth = 0;
        bool found_open = false;
        for (std::size_t j = i; j < parsed.tokens.size(); ++j) {
          if (parsed.tokens[j].text == "{") {
            ++depth;
            found_open = true;
          } else if (parsed.tokens[j].text == "}" && found_open) {
            --depth;
            if (depth == 0) {
              fn.body_token_end = j + 1;
              break;
            }
          }
        }
        break;
      }
    }

    functions.push_back(std::move(fn));
  }
  return functions;
}

} // namespace

DppTranslationUnit extract_from_parsed(const parser::ParsedSource &parsed) {
  DppTranslationUnit tu;
  tu.records = extract_records(parsed.text);
  tu.functions = extract_functions(parsed.text, parsed);
  return tu;
}

// ── analyze_scope implementation ─────────────────────────────────────────────

namespace {

using TokKind = parser::TokenKind;
using TokKW   = parser::KeywordKind;

// Return true if the '{' at brace_idx opens a struct/class body.
// Scans backward until hitting another { } ; (structural) token.
static bool scope_is_record_body(const std::vector<parser::Token> &T, std::size_t brace_idx) {
  for (std::size_t j = brace_idx; j-- > 0; ) {
    const auto &tok = T[j];
    if (tok.kind == TokKind::comment) continue;
    if (tok.kind == TokKind::punctuation &&
        (tok.text == "{" || tok.text == "}" || tok.text == ";"))
      return false;
    if (tok.kind == TokKind::keyword &&
        (tok.keyword == TokKW::struct_kw || tok.keyword == TokKW::class_kw))
      return true;
  }
  return false;
}

// Find the matching ')' for '(' at open_idx.
static std::size_t scope_find_close_paren(const std::vector<parser::Token> &T,
                                          std::size_t open_idx) {
  int depth = 1;
  for (std::size_t k = open_idx + 1; k < T.size(); ++k) {
    if (T[k].kind == TokKind::punctuation) {
      if (T[k].text == "(") ++depth;
      else if (T[k].text == ")") { --depth; if (depth == 0) return k; }
    }
  }
  return T.size();
}

// Check if a primitive keyword is a type keyword.
static bool is_type_kw(TokKW kw) {
  return kw == TokKW::int_kw || kw == TokKW::char_kw || kw == TokKW::void_kw ||
         kw == TokKW::bool_kw || kw == TokKW::double_kw || kw == TokKW::float_kw ||
         kw == TokKW::long_kw;
}

} // anonymous namespace

ScopeAnalysis analyze_scope(const std::vector<parser::Token> &tokens,
                            std::size_t begin, std::size_t end) {
  if (end > tokens.size()) end = tokens.size();
  ScopeAnalysis result;

  std::size_t depth = 0;
  std::vector<bool> scope_is_record; // one bool per depth level
  std::size_t last_nc = std::numeric_limits<std::size_t>::max();  // last non-comment token index

  for (std::size_t i = begin; i < end; ++i) {
    const auto &tok = tokens[i];
    if (tok.kind == TokKind::end_of_file) break;
    if (tok.kind == TokKind::comment) continue;

    // Track opening braces
    if (tok.kind == TokKind::punctuation && tok.text == "{") {
      const bool is_rec = scope_is_record_body(tokens, i);
      scope_is_record.push_back(is_rec);
      ++depth;
      last_nc = i;
      continue;
    }

    // Track closing braces
    if (tok.kind == TokKind::punctuation && tok.text == "}") {
      if (!scope_is_record.empty()) scope_is_record.pop_back();
      if (depth > 0) --depth;
      last_nc = i;
      continue;
    }

    // Only attempt declaration matching at "statement start" positions:
    // after { } ; : or at the very start (last_nc == npos).
    // Preprocessor directives (#include etc.) also act as statement separators.
    const bool at_stmt_start =
        last_nc == std::numeric_limits<std::size_t>::max() ||
        tokens[last_nc].kind == TokKind::preprocessor ||
        (tokens[last_nc].kind == TokKind::punctuation &&
         (tokens[last_nc].text == "{" || tokens[last_nc].text == "}" ||
          tokens[last_nc].text == ";" || tokens[last_nc].text == ":"));

    last_nc = i;

    if (!at_stmt_start) continue;

    // Try to match: [const] [static] [std::] type_name name [;|=|(]
    std::size_t j = i;
    std::string type_str;

    // Optional leading const
    if (j < end && tokens[j].kind == TokKind::keyword && tokens[j].keyword == TokKW::const_kw) {
      type_str = "const ";
      ++j;
      while (j < end && tokens[j].kind == TokKind::comment) ++j;
    }

    // Optional static
    if (j < end && tokens[j].kind == TokKind::keyword && tokens[j].keyword == TokKW::static_kw) {
      type_str += "static ";
      ++j;
      while (j < end && tokens[j].kind == TokKind::comment) ++j;
    }

    // Match type name: std::ident  OR  bare ident  OR  keyword type
    bool matched_type = false;
    if (j + 2 < end &&
        tokens[j].kind == TokKind::identifier && tokens[j].text == "std" &&
        tokens[j+1].kind == TokKind::oper && tokens[j+1].text == "::" &&
        tokens[j+2].kind == TokKind::identifier) {
      type_str += "std::" + tokens[j+2].text;
      j += 3;
      matched_type = true;
    } else if (j < end && tokens[j].kind == TokKind::identifier) {
      type_str += tokens[j].text;
      j += 1;
      matched_type = true;
    } else if (j < end && tokens[j].kind == TokKind::keyword && is_type_kw(tokens[j].keyword)) {
      type_str += tokens[j].text;
      j += 1;
      matched_type = true;
    }

    if (!matched_type) continue;
    while (j < end && tokens[j].kind == TokKind::comment) ++j;

    // Skip pointer/reference decorators
    while (j < end && tokens[j].kind == TokKind::oper &&
           (tokens[j].text == "*" || tokens[j].text == "&")) {
      type_str += tokens[j].text;
      ++j;
    }
    while (j < end && tokens[j].kind == TokKind::comment) ++j;

    // Next must be an identifier (the declared name)
    if (j >= end || tokens[j].kind != TokKind::identifier) continue;
    const std::string var_name = tokens[j].text;
    ++j;
    while (j < end && tokens[j].kind == TokKind::comment) ++j;

    if (j >= end) continue;

    bool is_fn = false;

    if (tokens[j].kind == TokKind::punctuation && tokens[j].text == "(") {
      // Look ahead past matching ) for [const] { to determine if function def.
      const std::size_t cp = scope_find_close_paren(tokens, j);
      if (cp >= end) continue;
      std::size_t k = cp + 1;
      while (k < end && tokens[k].kind == TokKind::comment) ++k;
      // Optional const qualifier
      if (k < end && tokens[k].kind == TokKind::keyword && tokens[k].keyword == TokKW::const_kw)
        ++k;
      while (k < end && tokens[k].kind == TokKind::comment) ++k;
      if (k < end && tokens[k].kind == TokKind::punctuation && tokens[k].text == "{") {
        is_fn = true;
      } else {
        continue; // function declaration (no body), skip
      }
    } else if (tokens[j].kind == TokKind::punctuation &&
               (tokens[j].text == ";" || tokens[j].text == "=")) {
      // Variable declaration
    } else {
      continue;
    }

    const bool cur_is_field = !scope_is_record.empty() && scope_is_record.back();

    ScopeVar sv;
    sv.type = type_str;
    sv.name = var_name;
    sv.scope_depth = depth;
    sv.decl_begin = tok.range.begin.offset;
    sv.is_field = cur_is_field;
    sv.is_function = is_fn;
    result.vars.push_back(std::move(sv));
  }

  return result;
}

} // namespace dpp::ir
