#pragma once

#include "dpp/parser/parser.h"

#include <string>
#include <vector>

namespace dpp::parser {

std::vector<Token> lex_translation_unit(const std::string &source);

} // namespace dpp::parser
