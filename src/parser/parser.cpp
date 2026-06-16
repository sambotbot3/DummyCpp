#include "dpp/parser/parser.h"

namespace dpp::parser {

ParsedSource parse_translation_unit(const std::string &source) {
  return ParsedSource{source};
}

} // namespace dpp::parser

