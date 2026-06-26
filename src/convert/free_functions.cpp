#include "dpp/convert/free_functions.h"
#include "dpp/string_utils.h"

#include <cctype>
#include <cstring>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

bool is_ident_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Returns true for lines that cannot be a top-level free function definition.
bool is_skipped_line(const std::string &line) {
  if (line.empty() || line[0] == ' ' || line[0] == '\t') return true;
  const char c0 = line[0];
  if (c0 == '/' || c0 == '#' || c0 == '*' || c0 == '}' || c0 == '{' || c0 == '"')
    return true;
  // Keywords with word boundaries
  static const char *const kws[] = {
      "static",    "inline",   "typedef", "struct", "class",   "enum",
      "namespace", "template", "using",   "if",     "for",     "while",
      "switch",    "return",   "else",    "do",     "extern",  "register",
      "volatile",  "auto",     "virtual", "operator", nullptr};
  for (int i = 0; kws[i]; ++i) {
    const std::size_t n = std::strlen(kws[i]);
    if (line.compare(0, n, kws[i]) == 0 &&
        (line.size() == n || !is_ident_char(line[n])))
      return true;
  }
  return false;
}

// Parse a function signature line into (return_type, name, params_raw).
// Accepts lines ending with '{' (definition) or ';' (declaration).
bool parse_func_sig(const std::string &line, std::string &ret, std::string &name,
                    std::string &params_raw) {
  const std::size_t paren_open = line.find('(');
  if (paren_open == std::string::npos || paren_open == 0) return false;

  // Use rfind ')' so params with nested parens (e.g. function pointers) are handled
  const std::size_t paren_close = line.rfind(')');
  if (paren_close == std::string::npos || paren_close < paren_open) return false;

  const std::string after = trim(line.substr(paren_close + 1));
  if (after.empty() || (after != ";" && after[0] != '{')) return false;

  // Extract name: last identifier in the text before '('
  const std::string before = line.substr(0, paren_open);
  std::size_t i = before.size();
  while (i > 0 && before[i - 1] == ' ') --i;
  const std::size_t name_end = i;
  while (i > 0 && is_ident_char(before[i - 1])) --i;
  const std::size_t name_start = i;
  if (name_start == name_end) return false;
  if (!std::isalpha(static_cast<unsigned char>(before[name_start])) &&
      before[name_start] != '_')
    return false;

  name = before.substr(name_start, name_end - name_start);
  ret = trim(before.substr(0, name_start));
  params_raw = line.substr(paren_open + 1, paren_close - paren_open - 1);
  return !ret.empty();
}

// Strip the variable name from a parameter declaration.
// "int x"           → "int"
// "const int *ptr"  → "const int *"
// "double"          → "double"
std::string param_type(const std::string &param) {
  const std::string p = trim(param);
  if (p.empty()) return "";
  std::size_t end = p.size();
  while (end > 0 && is_ident_char(p[end - 1])) --end;
  if (end == 0 || end == p.size()) return p;
  if (!std::isalpha(static_cast<unsigned char>(p[end])) && p[end] != '_') return p;
  return trim(p.substr(0, end));
}

// Build canonical signature string: "return_type(param_type0,param_type1,...)".
std::string build_signature(const std::string &ret, const std::string &params_raw) {
  std::ostringstream sig;
  sig << trim(ret) << "(";
  const std::vector<std::string> parts = split_commas(params_raw);
  bool first = true;
  for (const std::string &p : parts) {
    const std::string t = param_type(p);
    if (t.empty()) continue;
    if (!first) sig << ",";
    sig << t;
    first = false;
  }
  sig << ")";
  return sig.str();
}

// Lower "const T &name" → "const T name" in a single parameter string.
// Passing const refs as values is semantically equivalent for callers/callees when
// the function cannot modify through the reference.
std::string lower_const_ref_param(const std::string &param) {
  const std::string p = trim(param);
  static const std::regex re(R"(const\s+([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*)\s*)");
  std::smatch m;
  if (std::regex_match(p, m, re)) {
    return "const " + m[1].str() + " " + m[2].str();
  }
  return p;
}

// Find the ')' that closes the parameter-list '(' at open_pos,
// stopping if a '{' is seen at depth 0 (that would be a brace-enclosed body).
std::size_t matching_param_close(const std::string &s, std::size_t open_pos) {
  int depth = 0;
  for (std::size_t i = open_pos; i < s.size(); ++i) {
    if (s[i] == '(') {
      ++depth;
    } else if (s[i] == ')') {
      --depth;
      if (depth == 0) return i;
    } else if (s[i] == '{' && depth == 0) {
      return std::string::npos;
    }
  }
  return std::string::npos;
}

// If the line is a function declaration/definition with const-ref params, rewrite them.
std::string lower_const_ref_params_in_line(const std::string &line) {
  if (line.find('&') == std::string::npos) return line;

  // Find the first '(' that opens the parameter list.
  // Skip lines that start with a keyword — is_skipped_line() guards callers already,
  // but be defensive: require an identifier character before '('.
  const std::size_t paren_open = line.find('(');
  if (paren_open == 0 || paren_open == std::string::npos) return line;

  // Check that there's an identifier immediately before '(': must be a function name.
  {
    std::size_t j = paren_open;
    while (j > 0 && line[j - 1] == ' ') --j;
    if (j == 0 || !is_ident_char(line[j - 1])) return line;
  }

  const std::size_t paren_close = matching_param_close(line, paren_open);
  if (paren_close == std::string::npos) return line;

  const std::string params_raw = line.substr(paren_open + 1, paren_close - paren_open - 1);
  if (params_raw.find('&') == std::string::npos) return line;

  std::vector<std::string> parts = split_commas(params_raw);
  bool changed = false;
  for (std::string &p : parts) {
    const std::string lowered = lower_const_ref_param(p);
    if (lowered != trim(p)) {
      p = lowered;
      changed = true;
    }
  }
  if (!changed) return line;

  std::string new_params;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) new_params += ", ";
    new_params += parts[i];
  }
  return line.substr(0, paren_open + 1) + new_params + line.substr(paren_close);
}

// Replace word-boundary occurrences of `orig` immediately followed by '(' with `mangled`.
std::string rename_calls(const std::string &source, const std::string &orig,
                         const std::string &mangled) {
  std::string out;
  out.reserve(source.size());
  const std::size_t n = orig.size();
  std::size_t i = 0;
  while (i < source.size()) {
    if (i + n <= source.size() && source.compare(i, n, orig) == 0) {
      const bool left_ok = (i == 0) || !is_ident_char(source[i - 1]);
      std::size_t j = i + n;
      while (j < source.size() && (source[j] == ' ' || source[j] == '\t')) ++j;
      const bool right_ok = j < source.size() && source[j] == '(';
      if (left_ok && right_ok) {
        out += mangled;
        i += n;
        continue;
      }
    }
    out += source[i++];
  }
  return out;
}

} // namespace

FreeFunctionResult lower_free_functions(const std::string &source) {
  FreeFunctionResult result;

  // Collect function definitions and declarations.
  struct FuncDef {
    std::string name;
    std::string ret;
    std::string params_raw;
  };
  std::vector<FuncDef> defs;

  for (const std::string &line : split_lines(source)) {
    if (is_skipped_line(line)) continue;
    std::string name, ret, params;
    if (!parse_func_sig(line, ret, name, params)) continue;
    if (name == "main") continue;
    defs.push_back({name, ret, params});
  }

  // Group signatures by name to detect overloads.
  // A declaration + definition pair has the same signature → single unique signature.
  std::map<std::string, std::set<std::string>> name_sigs;
  for (const FuncDef &d : defs)
    name_sigs[d.name].insert(build_signature(d.ret, d.params_raw));

  // Build rename map: only mangle names with exactly one unique signature.
  // Overloaded names (multiple signatures) are deferred until IR enables type-aware
  // call-site resolution. (TODO)
  std::map<std::string, std::string> rename_map;
  for (const auto &[name, sigs] : name_sigs) {
    if (sigs.size() != 1) continue;
    const std::string &sig = *sigs.begin();
    rename_map[name] = name + "_" + fnv1a_32_hex(sig);
  }

  std::string out = source;
  for (const auto &[orig, mangled] : rename_map)
    out = rename_calls(out, orig, mangled);

  // Rewrite const-ref params in function signatures.
  std::vector<std::string> lines = split_lines(out);
  bool any_ref = false;
  for (std::string &line : lines) {
    if (!is_skipped_line(line) && line.find('&') != std::string::npos) {
      line = lower_const_ref_params_in_line(line);
      any_ref = true;
    }
  }
  if (!rename_map.empty() || any_ref) {
    result.source = join_lines(lines);
  } else {
    result.source = source;
  }
  return result;
}

} // namespace dpp::convert
