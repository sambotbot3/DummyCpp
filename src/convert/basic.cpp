#include "dpp/convert/basic.h"

#include <cctype>
#include <regex>
#include <sstream>
#include <string>

namespace dpp::convert {

// Infer the C type for an `auto` variable from its initializer expression.
// Returns the inferred type string, or "" if the type cannot be determined.
static std::string infer_auto_type(const std::string &init) {
  const std::string s = [&] {
    std::size_t i = 0, j = init.size();
    while (i < j && std::isspace(static_cast<unsigned char>(init[i]))) ++i;
    while (j > i && std::isspace(static_cast<unsigned char>(init[j - 1]))) --j;
    return init.substr(i, j - i);
  }();

  if (s.empty()) return "";

  // String literal
  if (s.front() == '"') return "const char *";
  // Char literal
  if (s.front() == '\'') return "char";
  // Boolean keywords
  if (s == "true" || s == "false") return "int";

  // Numeric: check for float indicators
  bool has_dot = false;
  bool has_exp = false;
  bool all_numeric = true;
  std::size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
  for (std::size_t i = start; i < s.size(); ++i) {
    const char c = s[i];
    if (c == '.') { has_dot = true; }
    else if (c == 'e' || c == 'E') { has_exp = true; }
    else if (c == 'f' || c == 'F' || c == 'l' || c == 'L' || c == 'u' || c == 'U') {
      // suffix — ok, still numeric
    } else if (!std::isdigit(static_cast<unsigned char>(c))) {
      all_numeric = false;
    }
  }

  if (all_numeric && start < s.size()) {
    if (has_dot || has_exp) return "double";
    // Check for L/UL suffix → long, otherwise int
    const char last = std::toupper(static_cast<unsigned char>(s.back()));
    if (last == 'L') return "long";
    if (last == 'U') return "unsigned int";
    return "int";
  }

  return "";
}

std::string lower_auto_types(const std::string &source) {
  static const std::regex auto_re(
      R"(^(\s*)(const\s+)?auto\s+([A-Za-z_]\w*)\s*=\s*(.+)\s*;\s*$)");
  std::istringstream in(source);
  std::ostringstream out;
  std::string line;
  while (std::getline(in, line)) {
    std::smatch m;
    if (std::regex_match(line, m, auto_re)) {
      const std::string indent = m[1].str();
      const std::string cv = m[2].str();
      const std::string varname = m[3].str();
      const std::string init = m[4].str();
      const std::string inferred = infer_auto_type(init);
      if (!inferred.empty()) {
        // Don't prepend cv qualifier if the inferred type already has const
        const bool type_has_const = inferred.find("const") != std::string::npos;
        const std::string type_str = (type_has_const || cv.empty()) ? inferred : cv + inferred;
        out << indent << type_str << " " << varname << " = " << init << ";\n";
        continue;
      }
    }
    out << line << '\n';
  }
  return out.str();
}

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
