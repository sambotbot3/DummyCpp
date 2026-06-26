#include "dpp/convert/basic.h"
#include "dpp/string_utils.h"

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

// Split an enum body on top-level commas, returning (name, value_suffix) pairs.
static std::vector<std::pair<std::string, std::string>>
parse_enum_entries(const std::string &body) {
  std::vector<std::pair<std::string, std::string>> result;
  std::size_t start = 0;
  int depth = 0;
  for (std::size_t i = 0; i <= body.size(); ++i) {
    const char c = (i < body.size()) ? body[i] : ','; // sentinel
    if (c == '(' || c == '{') { ++depth; continue; }
    if (c == ')' || c == '}') { if (depth > 0) --depth; continue; }
    if (c == ',' && depth == 0) {
      const std::string entry = trim(body.substr(start, i - start));
      start = i + 1;
      if (entry.empty()) continue;
      const auto eq = entry.find('=');
      if (eq == std::string::npos) {
        result.push_back({entry, ""});
      } else {
        result.push_back({trim(entry.substr(0, eq)), " " + trim(entry.substr(eq))});
      }
    }
  }
  return result;
}

std::string lower_enums(const std::string &source) {
  struct EnumClass {
    std::string name;
    std::vector<std::string> enumerator_names;
  };
  std::vector<EnumClass> enum_classes;

  // Collect enum class info before any rewriting.
  static const std::regex ec_scan_re(
      R"(\benum\s+class\s+([A-Za-z_]\w*)\s*(?::\s*[A-Za-z_][\w:]*\s*)?\{([^}]*)\}\s*;)");
  for (std::sregex_iterator it(source.begin(), source.end(), ec_scan_re);
       it != std::sregex_iterator(); ++it) {
    EnumClass ec;
    ec.name = (*it)[1].str();
    for (const auto &e : parse_enum_entries((*it)[2].str()))
      ec.enumerator_names.push_back(e.first);
    enum_classes.push_back(ec);
  }

  std::string out = source;

  // Plain enum → typedef enum (excludes enum class via negative lookahead).
  out = std::regex_replace(
      out,
      std::regex(R"(\benum\s+(?!class\b)([A-Za-z_]\w*)\s*\{([^}]*)\}\s*;)"),
      "typedef enum $1 {$2} $1;");

  // Rewrite enum class declarations in reverse order (to keep offsets valid).
  {
    std::vector<std::smatch> matches;
    for (std::sregex_iterator it(out.begin(), out.end(), ec_scan_re);
         it != std::sregex_iterator(); ++it)
      matches.push_back(*it);
    for (int i = static_cast<int>(matches.size()) - 1; i >= 0; --i) {
      const std::smatch &m = matches[i];
      const std::string enum_name = m[1].str();
      const auto entries = parse_enum_entries(m[2].str());
      std::string new_body;
      for (std::size_t j = 0; j < entries.size(); ++j) {
        if (j > 0) new_body += ", ";
        new_body += enum_name + "_" + entries[j].first;
        if (!entries[j].second.empty()) new_body += entries[j].second;
      }
      const std::string repl =
          "typedef enum " + enum_name + "_e { " + new_body + " } " + enum_name + ";";
      out.replace(m.position(), m.length(), repl);
    }
  }

  // Replace all EnumName::Enumerator references.
  for (const auto &ec : enum_classes) {
    for (const auto &en : ec.enumerator_names) {
      out = std::regex_replace(
          out, std::regex("\\b" + ec.name + "::" + en + "\\b"), ec.name + "_" + en);
    }
  }

  return out;
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
  // Strip C++ compat includes that have no direct C counterpart needed.
  static const char *const compat_includes[] = {
      "#include <cstddef>",  "#include <cstdlib>",   "#include <cstring>",
      "#include <cmath>",    "#include <climits>",   "#include <utility>",
      "#include <type_traits>", "#include <functional>", "#include <numeric>",
      "#include <stdexcept>", nullptr};

  std::string out;
  {
    std::istringstream in(source);
    std::ostringstream oss;
    std::string ln;
    while (std::getline(in, ln)) {
      const std::string s = trim(ln);
      bool skip = false;
      for (int i = 0; compat_includes[i]; ++i) {
        if (s == compat_includes[i]) { skip = true; break; }
      }
      if (!skip) oss << ln << '\n';
    }
    out = oss.str();
  }
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
  out = std::regex_replace(out, std::regex(R"(\bnullptr\b)"), "NULL");
  out = std::regex_replace(out, std::regex(R"(\bstd::string::npos\b)"), "((size_t)-1)");
  out = std::regex_replace(out, std::regex(R"(\bconstexpr\s+)"), "const ");
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
