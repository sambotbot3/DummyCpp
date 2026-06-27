#pragma once

#include "dpp/parser/parser.h"

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace dpp::ir {

struct Field {
  std::string type;
  std::string name;
};

struct Param {
  std::string type;
  std::string name;
};

struct DppRecord {
  std::string name;
  std::vector<std::string> bases;
  std::vector<Field> fields;
  std::vector<std::string> method_names;
};

struct DppFunction {
  std::string name;
  std::string return_type;
  std::vector<Param> params;
  // Token range into ParsedSource::tokens for the body (begin index, past-end index).
  std::size_t body_token_begin = 0;
  std::size_t body_token_end = 0;
  bool is_method = false;
  bool is_constructor = false;
};

struct DppTranslationUnit {
  std::vector<DppRecord> records;
  std::vector<DppFunction> functions;
};

// Populate a DppTranslationUnit from the original parsed C++ source.
// Extracts struct/class definitions and free function definitions.
// The IR reflects the C++ input before any lowering passes.
DppTranslationUnit extract_from_parsed(const parser::ParsedSource &parsed);

// Scope variable / function declaration found by analyze_scope.
struct ScopeVar {
  std::string type;        // type string (e.g. "std::string", "dpp_string", "int")
  std::string name;        // declared name
  std::size_t scope_depth = 0;
  std::size_t decl_begin = 0;  // byte offset of first token of declaration
  bool is_field = false;       // true if directly inside a struct/class body
  bool is_function = false;    // true if this is a function definition (type = return type)
};

struct ScopeAnalysis {
  std::vector<ScopeVar> vars;
};

// Walk tokens[begin..end), track brace depth, collect variable/function declarations.
// Recognises: [const] [std::] ident name [= | ( | ;]
// For `(` delimiter: looks ahead past matching `)` for `{` to detect function definitions.
// Sets is_field=true for decls directly inside struct/class bodies.
// Sets is_function=true when the token after fn_name( ... ) [const] is `{`.
ScopeAnalysis analyze_scope(const std::vector<parser::Token> &tokens,
                            std::size_t begin = 0,
                            std::size_t end = std::numeric_limits<std::size_t>::max());

} // namespace dpp::ir
