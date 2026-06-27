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

// Infer C type of a single call argument (literal or known variable).
static std::string infer_arg_type(const std::string &arg,
                                   const std::map<std::string, std::string> &var_types) {
  const std::string a = trim(arg);
  if (a.empty()) return "";
  if (a.front() == '"') return "const char *";
  if (a.front() == '\'') return "char";
  if (a == "true" || a == "false") return "int";
  bool has_dot = false, has_e = false, all_num = true;
  const std::size_t start = (a[0] == '-' || a[0] == '+') ? 1 : 0;
  for (std::size_t k = start; k < a.size(); ++k) {
    const char c = a[k];
    if (c == '.') has_dot = true;
    else if (c == 'e' || c == 'E') has_e = true;
    else if (c != 'f' && c != 'F' && c != 'l' && c != 'L' && c != 'u' && c != 'U' &&
             !std::isdigit(static_cast<unsigned char>(c))) all_num = false;
  }
  if (all_num && start < a.size()) return (has_dot || has_e) ? "double" : "int";
  const auto it = var_types.find(a);
  if (it != var_types.end()) return it->second;
  return "";
}

// Check whether inferred arg type is compatible with a declared param type.
static bool types_compatible(const std::string &arg_t, const std::string &param_t) {
  if (arg_t == param_t) return true;
  if (arg_t == "int" && (param_t == "long" || param_t == "unsigned int" || param_t == "double"))
    return true;
  if (arg_t == "double" && (param_t == "float")) return true;
  return false;
}

// Extract normalized list of param types from a raw param string.
static std::vector<std::string> extract_param_types(const std::string &params_raw) {
  std::vector<std::string> result;
  for (const std::string &p : split_commas(params_raw)) {
    const std::string t = param_type(p);
    if (!t.empty()) result.push_back(t);
  }
  return result;
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
      const bool right_ok = (i + n >= source.size()) || !is_ident_char(source[i + n]);
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
    std::string sig;                        // build_signature output
    std::string mangled;                    // mangled name
    std::vector<std::string> param_types;   // per-param types
    int arity = 0;
  };
  std::vector<FuncDef> defs;

  for (const std::string &line : split_lines(source)) {
    if (is_skipped_line(line)) continue;
    std::string name, ret, params;
    if (!parse_func_sig(line, ret, name, params)) continue;
    if (name == "main") continue;
    FuncDef d;
    d.name = name;
    d.ret = ret;
    d.params_raw = params;
    d.sig = build_signature(ret, params);
    d.param_types = extract_param_types(params);
    d.arity = static_cast<int>(d.param_types.size());
    d.mangled = name + "_" + fnv1a_32_hex(d.sig);
    defs.push_back(d);
  }

  // Group signatures by name; deduplicate (decl + def count as one).
  std::map<std::string, std::vector<FuncDef>> name_to_defs;
  {
    std::map<std::string, std::set<std::string>> seen; // name → set of sigs already added
    for (const FuncDef &d : defs) {
      if (seen[d.name].insert(d.sig).second)
        name_to_defs[d.name].push_back(d);
    }
  }

  // Build simple rename map (non-overloaded) and overload table.
  std::map<std::string, std::string> rename_map;  // orig → single mangled
  struct OverloadSet {
    std::vector<FuncDef> variants;  // unique signatures, sorted by arity
  };
  std::map<std::string, OverloadSet> overload_map;

  for (const auto &[name, variants] : name_to_defs) {
    if (variants.size() == 1) {
      rename_map[name] = variants[0].mangled;
    } else {
      overload_map[name].variants = variants;
      // Also register each variant's definition mangled name so the definition itself is renamed.
      for (const FuncDef &v : variants)
        rename_map[v.mangled + "__overload_decl__" + v.sig] = v.mangled; // placeholder, handled below
    }
  }

  // Pass 1: rename non-overloaded calls and definitions.
  std::string out = source;
  for (const auto &[orig, mangled] : rename_map) {
    if (orig.find("__overload_decl__") != std::string::npos) continue;
    out = rename_calls(out, orig, mangled);
  }

  // Pass 2: rewrite overloaded function definitions by signature-matching each line.
  // At each function signature line, determine which overload it is and rename it.
  // At call sites, count args and infer types to pick the right overload.
  if (!overload_map.empty()) {
    // Pre-scan for known variable types (for type inference at call sites).
    std::map<std::string, std::string> var_types;
    {
      static const std::regex vt_re(
          R"(^\s*(?:const\s+)?(int|double|float|long|char|unsigned(?:\s+int)?|size_t)\s+([A-Za-z_]\w*)\s*[=;])");
      for (const std::string &ln : split_lines(out)) {
        std::smatch vm;
        if (std::regex_search(ln, vm, vt_re)) var_types[vm[2].str()] = vm[1].str();
      }
    }

    std::vector<std::string> lines2 = split_lines(out);
    for (std::string &line : lines2) {
      // Rename overloaded function definitions/declarations.
      if (!is_skipped_line(line)) {
        std::string def_name, def_ret, def_params;
        if (parse_func_sig(line, def_ret, def_name, def_params)) {
          const auto oit = overload_map.find(def_name);
          if (oit != overload_map.end()) {
            const std::string def_sig = build_signature(def_ret, def_params);
            for (const FuncDef &v : oit->second.variants) {
              if (v.sig == def_sig) {
                line = rename_calls(line, def_name, v.mangled);
                break;
              }
            }
          }
        }
      }

      // Rename overloaded call sites by resolving argument types.
      for (const auto &[name, oset] : overload_map) {
        // Scan for call sites: name followed by (
        std::string result2;
        std::size_t pos = 0;
        while (pos < line.size()) {
          const std::size_t found = line.find(name, pos);
          if (found == std::string::npos) { result2 += line.substr(pos); break; }
          // Check word boundaries.
          const bool left_ok = (found == 0) || !is_ident_char(line[found - 1]);
          const bool right_ok_end = found + name.size() < line.size();
          if (!left_ok || !right_ok_end || is_ident_char(line[found + name.size()])) {
            result2 += line[pos];
            ++pos;
            continue;
          }
          // Check for '(' after optional spaces.
          std::size_t j = found + name.size();
          while (j < line.size() && line[j] == ' ') ++j;
          if (j >= line.size() || line[j] != '(') {
            result2 += line.substr(pos, found - pos + name.size());
            pos = found + name.size();
            continue;
          }
          // Find matching close paren.
          const std::size_t open = j;
          int depth = 0;
          std::size_t close = open;
          for (std::size_t k = open; k < line.size(); ++k) {
            if (line[k] == '(') ++depth;
            else if (line[k] == ')') { --depth; if (depth == 0) { close = k; break; } }
          }
          const std::string args_str = line.substr(open + 1, close - open - 1);
          const bool args_empty = trim(args_str).empty();
          const auto arg_list = args_empty ? std::vector<std::string>{} : split_commas(args_str);
          const int nargs = static_cast<int>(arg_list.size());

          // Find best matching overload.
          const FuncDef *best = nullptr;
          int best_score = -1;
          for (const FuncDef &v : oset.variants) {
            if (v.arity != nargs) continue;
            int score = 0;
            for (int ai = 0; ai < nargs; ++ai) {
              const std::string at = infer_arg_type(arg_list[ai], var_types);
              const std::string pt = (ai < (int)v.param_types.size()) ? v.param_types[ai] : "";
              if (!at.empty() && !pt.empty()) {
                if (at == pt) score += 2;
                else if (types_compatible(at, pt)) score += 1;
              }
            }
            if (score > best_score) { best_score = score; best = &v; }
          }
          result2 += line.substr(pos, found - pos);
          result2 += (best ? best->mangled : name);
          result2 += line.substr(j, close - j + 1);
          pos = close + 1;
        }
        line = result2;
      }
    }
    out = join_lines(lines2);
  }

  // Rewrite const-ref params in function signatures.
  std::vector<std::string> lines = split_lines(out);
  bool any_ref = false;
  for (std::string &line : lines) {
    if (!is_skipped_line(line) && line.find('&') != std::string::npos) {
      line = lower_const_ref_params_in_line(line);
      any_ref = true;
    }
  }
  if (!rename_map.empty() || !overload_map.empty() || any_ref) {
    result.source = join_lines(lines);
  } else {
    result.source = source;
  }
  return result;
}

} // namespace dpp::convert
