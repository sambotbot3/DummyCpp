#include "dpp/convert/basic.h"

#include <regex>

namespace dpp::convert {

std::string lower_structs(const std::string &source) {
  const std::regex struct_re(R"(struct\s+([A-Za-z_]\w*)\s*\{([^}]*)\}\s*;)");
  return std::regex_replace(source, struct_re, "typedef struct $1 {$2} $1;");
}

std::string lower_main_signature(const std::string &source) {
  const std::regex main_re(R"(\bint\s+main\s*\(\s*\))");
  return std::regex_replace(source, main_re, "int main(void)");
}

std::string lower_aggregate_initializers(const std::string &source) {
  const std::regex aggregate_re(
      R"(\b([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*\{([^{};]*)\}\s*;)");
  return std::regex_replace(source, aggregate_re, "$1 $2 = ($1){$3};");
}

std::string lower_cpp_surface_types(const std::string &source) {
  std::string out = source;
  out = std::regex_replace(
      out,
      std::regex(
          R"(\btypedef\s+(?:std::)?map\s*<\s*(?:std::string|string|[A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;)"),
      "typedef dpp_map $2;");
  out = std::regex_replace(
      out,
      std::regex(R"(\btypedef\s+(?:std::)?vector\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;)"),
      "typedef dpp_vector $2;");
  out = std::regex_replace(
      out,
      std::regex(R"(\b(?:std::)?map\s*<\s*(?:std::string|string|[A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*>)"),
      "dpp_map");
  out = std::regex_replace(
      out, std::regex(R"(\b(?:std::)?vector\s*<\s*([A-Za-z_]\w*)\s*>)"), "dpp_vector");
  out = std::regex_replace(out, std::regex(R"(\bstd::string\b)"), "dpp_string");
  out = std::regex_replace(
      out, std::regex(R"(\bconst\s+dpp_string\s*&\s*([A-Za-z_]\w*))"), "const char *$1");
  out = std::regex_replace(
      out, std::regex(R"(\bconst\s+([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*))"), "const $1 *$2");
  out = std::regex_replace(
      out, std::regex(R"(\b([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*))"), "$1 *$2");
  out = std::regex_replace(out, std::regex(R"(static_cast\s*<\s*([^>]+)\s*>\s*\(([^;]*)\))"),
                           "(($1)($2))");
  out = std::regex_replace(out, std::regex(R"(\b([A-Za-z_]\w*)\.size\s*\(\s*\))"),
                           "dpp_vector_size(&$1)");
  return out;
}

} // namespace dpp::convert
