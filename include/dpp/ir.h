#pragma once

#include "dpp/parser/parser.h"

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

} // namespace dpp::ir
