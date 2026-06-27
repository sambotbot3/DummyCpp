#include "dpp/convert/basic.h"
#include "dpp/string_utils.h"

#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace dpp::convert {

// ─── Helper: split a raw parameter list on top-level commas ─────────────────
static std::vector<std::string> split_param_list(const std::string &params) {
  std::vector<std::string> result;
  std::string cur;
  int depth = 0;
  for (char c : params) {
    if (c == '(' || c == '<' || c == '[' || c == '{') { ++depth; cur += c; }
    else if (c == ')' || c == '>' || c == ']' || c == '}') { --depth; cur += c; }
    else if (c == ',' && depth == 0) {
      const std::string t = trim(cur);
      if (!t.empty()) result.push_back(t);
      cur.clear();
    } else {
      cur += c;
    }
  }
  const std::string t = trim(cur);
  if (!t.empty()) result.push_back(t);
  return result;
}

// ─── Helper: find closing paren matching the open paren at open_pos ──────────
static std::size_t find_matching_paren(const std::string &s, std::size_t open_pos) {
  int depth = 0;
  for (std::size_t i = open_pos; i < s.size(); ++i) {
    if (s[i] == '(') ++depth;
    else if (s[i] == ')') { --depth; if (depth == 0) return i; }
  }
  return std::string::npos;
}

// ─── Helper: count comma-separated args at the top nesting level ─────────────
static int count_call_args(const std::string &args) {
  const std::string t = trim(args);
  if (t.empty()) return 0;
  int count = 1, depth = 0;
  for (char c : t) {
    if (c == '(' || c == '<' || c == '[' || c == '{') ++depth;
    else if (c == ')' || c == '>' || c == ']' || c == '}') --depth;
    else if (c == ',' && depth == 0) ++count;
  }
  return count;
}

struct FnDefaultsInfo {
  int total_params = 0;
  std::vector<std::string> defaults; // per param; empty = required
};

// Find default= position inside a single param string (respecting < > ( ) nesting).
static int find_default_eq(const std::string &param) {
  int depth = 0;
  for (int i = 0; i < static_cast<int>(param.size()); ++i) {
    char c = param[i];
    if (c == '(' || c == '<' || c == '[') ++depth;
    else if (c == ')' || c == '>' || c == ']') --depth;
    else if (c == '=' && depth == 0) return i;
  }
  return -1;
}

// Expand call sites for functions with known defaults.
static std::string expand_default_calls(const std::string &line,
                                         const std::map<std::string, FnDefaultsInfo> &fn_defs) {
  std::string result;
  std::size_t i = 0;
  while (i < line.size()) {
    const char ch = line[i];
    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
      const std::size_t id_start = i;
      while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_'))
        ++i;
      const std::string ident = line.substr(id_start, i - id_start);
      // Skip spaces before '('
      std::size_t j = i;
      while (j < line.size() && line[j] == ' ') ++j;
      auto it = fn_defs.find(ident);
      if (it != fn_defs.end() && j < line.size() && line[j] == '(') {
        const std::size_t close = find_matching_paren(line, j);
        if (close != std::string::npos) {
          const std::string args_str = line.substr(j + 1, close - j - 1);
          const int nargs = count_call_args(args_str);
          const FnDefaultsInfo &info = it->second;
          if (nargs < info.total_params) {
            std::string new_args = args_str;
            for (int idx = nargs; idx < info.total_params; ++idx) {
              if (!info.defaults[idx].empty()) {
                if (!trim(new_args).empty()) new_args += ", ";
                new_args += info.defaults[idx];
              }
            }
            result += ident + "(" + new_args + ")";
          } else {
            result += ident + line.substr(j, close - j + 1);
          }
          i = close + 1;
          continue;
        }
      }
      result += ident;
    } else {
      result += line[i++];
    }
  }
  return result;
}

std::string lower_default_args(const std::string &source) {
  // Pass 1: collect function signatures that have default args.
  std::map<std::string, FnDefaultsInfo> fn_defaults;

  // Matches: RetType fn_name(params) [const] { or ;
  static const std::regex fn_decl_re(
      R"(^(\s*)(?:(?:const\s+)?[A-Za-z_][\w:]*(?:\s*\*)?)\s+([A-Za-z_]\w*)\s*\()");

  {
    std::istringstream pre(source);
    std::string pl;
    while (std::getline(pre, pl)) {
      if (pl.find('=') == std::string::npos) continue;
      std::smatch sm;
      if (!std::regex_search(pl, sm, fn_decl_re)) continue;
      const std::string fn_name = sm[2].str();
      const std::size_t open_pos = pl.find('(', sm.position());
      if (open_pos == std::string::npos) continue;
      const std::size_t close_pos = find_matching_paren(pl, open_pos);
      if (close_pos == std::string::npos) continue;
      const std::string raw_params = pl.substr(open_pos + 1, close_pos - open_pos - 1);
      if (raw_params.find('=') == std::string::npos) continue;

      const auto param_list = split_param_list(raw_params);
      FnDefaultsInfo info;
      info.total_params = static_cast<int>(param_list.size());
      bool has_any_default = false;
      for (const std::string &p : param_list) {
        const int eq_pos = find_default_eq(p);
        if (eq_pos >= 0) {
          info.defaults.push_back(trim(p.substr(eq_pos + 1)));
          has_any_default = true;
        } else {
          info.defaults.push_back("");
        }
      }
      if (has_any_default) fn_defaults[fn_name] = info;
    }
  }

  if (fn_defaults.empty()) return source;

  // Pass 2: strip defaults from definitions; expand call sites.
  std::istringstream in(source);
  std::ostringstream out;
  std::string line;
  while (std::getline(in, line)) {
    // First expand call sites (before we might strip from this line).
    std::string rewritten = expand_default_calls(line, fn_defaults);

    // Then check if the line is itself a definition/declaration with defaults to strip.
    std::smatch sm;
    if (rewritten.find('=') != std::string::npos &&
        std::regex_search(rewritten, sm, fn_decl_re)) {
      const std::string fn_name = sm[2].str();
      if (fn_defaults.count(fn_name)) {
        const std::size_t open_pos = rewritten.find('(', sm.position());
        const std::size_t close_pos = find_matching_paren(rewritten, open_pos);
        if (open_pos != std::string::npos && close_pos != std::string::npos) {
          const std::string raw_params = rewritten.substr(open_pos + 1, close_pos - open_pos - 1);
          if (raw_params.find('=') != std::string::npos) {
            const auto param_list = split_param_list(raw_params);
            std::string cleaned;
            for (int i = 0; i < static_cast<int>(param_list.size()); ++i) {
              if (i > 0) cleaned += ", ";
              const int eq_pos = find_default_eq(param_list[i]);
              cleaned += (eq_pos >= 0) ? trim(param_list[i].substr(0, eq_pos)) : param_list[i];
            }
            const std::string after = rewritten.substr(close_pos + 1);
            rewritten = rewritten.substr(0, open_pos + 1) + cleaned + ")" + after;
          }
        }
      }
    }

    out << rewritten << "\n";
  }
  return out.str();
}

// Sanitize a C++ type name to a C identifier fragment for pair struct naming.
static std::string pair_type_ident(const std::string &t) {
  if (t == "std::string" || t == "string" || t == "dpp_string") return "str";
  if (t == "double") return "dbl";
  if (t == "long") return "long";
  if (t == "float") return "float";
  std::string r;
  for (unsigned char c : t) r += std::isalnum(c) ? static_cast<char>(c) : '_';
  return r;
}

// Map a C++ pair member type to its C equivalent.
static std::string pair_c_type(const std::string &t) {
  if (t == "std::string" || t == "string") return "dpp_string";
  return t;
}

std::string lower_pairs(const std::string &source) {
  static const std::regex pair_re(
      R"(\bstd::pair\s*<\s*([A-Za-z_][\w:]*(?:\s*\*)?)\s*,\s*([A-Za-z_][\w:]*(?:\s*\*)?)\s*>)");

  // Collect unique pair type combinations.
  std::set<std::pair<std::string, std::string>> seen;
  std::vector<std::pair<std::string, std::string>> order; // preserve insertion order
  for (std::sregex_iterator it(source.begin(), source.end(), pair_re), end;
       it != end; ++it) {
    const std::string t1 = trim((*it)[1].str());
    const std::string t2 = trim((*it)[2].str());
    if (seen.insert({t1, t2}).second) order.push_back({t1, t2});
  }
  if (order.empty()) return source;

  // Build typedef structs.
  std::string typedefs;
  // Build a replacement map: pair_re match → struct name.
  // We'll do string replacements for each unique pair type.
  std::string out = source;
  for (const auto &[t1, t2] : order) {
    const std::string struct_name = "dpp_pair_" + pair_type_ident(t1) + "_" + pair_type_ident(t2);
    typedefs += "typedef struct { " + pair_c_type(t1) + " first; " +
                pair_c_type(t2) + " second; } " + struct_name + ";\n";
    // Escape for regex.
    auto re_escape = [](const std::string &s) {
      static const std::regex meta(R"([.^$|()\[\]{}*+?\\])");
      return std::regex_replace(s, meta, "\\$0");
    };
    const std::regex this_pair_re("\\bstd::pair\\s*<\\s*" + re_escape(t1) +
                                  "\\s*,\\s*" + re_escape(t2) + "\\s*>");
    out = std::regex_replace(out, this_pair_re, struct_name);
  }

  // Scan for pair-returning functions and fix `return {a, b}` → `return (T){a, b}`.
  // Build map: function_name → pair struct name from return type.
  std::map<std::string, std::string> fn_pair_return;
  static const std::regex fn_sig_re(
      R"(\b(dpp_pair_[A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*\([^)]*\)\s*\{)");
  for (std::sregex_iterator it(out.begin(), out.end(), fn_sig_re), end;
       it != end; ++it) {
    fn_pair_return[(*it)[2].str()] = (*it)[1].str();
  }
  // For each function body returning a pair type, rewrite `return {` → `return (T){`.
  // Simple line-by-line: when inside a pair-returning function, patch return statements.
  {
    std::vector<std::string> lines = split_lines(out);
    std::string current_pair_type;
    for (auto &line : lines) {
      // Detect function signature line.
      std::smatch sm;
      if (std::regex_search(line, sm, fn_sig_re)) {
        current_pair_type = sm[1].str();
      }
      if (!current_pair_type.empty()) {
        // Rewrite `return {` → `return (dpp_pair_...){`.
        static const std::regex ret_brace_re(R"(\breturn\s*\{)");
        if (std::regex_search(line, ret_brace_re)) {
          line = std::regex_replace(line, ret_brace_re,
                                    "return (" + current_pair_type + "){");
        }
        // Detect closing brace at function level (very rough: line == "}").
        if (trim(line) == "}") current_pair_type = "";
      }
    }
    out = join_lines(lines);
  }

  // std::make_pair(a, b) → {a, b} (compound init; caller's type context drives this).
  static const std::regex make_pair_re(R"(\bstd::make_pair\s*\()");
  out = std::regex_replace(out, make_pair_re, "(");
  // Fix double-paren: std::make_pair(x, y) → (x, y) — replace leading `(` of the call
  // with `{` and the closing `)` with `}`.
  // The above regex replaces `std::make_pair(` → `(`, which gives `(x, y)`.
  // We need `{x, y}` but that requires matching the closing paren.
  // Simpler: just strip `std::make_pair` and let the (x, y) remainder parse as C.
  // Actually, C doesn't accept (x, y) as an initializer. Rewrite as a proper aggregate.
  // Use a second pass to find remaining `(x, y)` patterns left by the above replacement:
  // This is getting complex; for now emit a comment and rely on the typedef.
  // TODO: properly fix std::make_pair(a, b) → (dpp_pair_T_U){a, b}

  // Strip `#include <utility>` — handled by lower_cpp_surface_types but do it here too.
  out = std::regex_replace(out, std::regex(R"(#include\s*<utility>\s*\n)"), "");

  // Insert typedef block after last #include line (or at top if none).
  {
    std::size_t insert_pos = 0;
    static const std::regex include_re(R"(#include\s*[<"][^>"]*[>"])");
    for (std::sregex_iterator it(out.begin(), out.end(), include_re), end;
         it != end; ++it) {
      insert_pos = static_cast<std::size_t>((*it).position() + (*it).length());
      // Advance past newline.
      while (insert_pos < out.size() && out[insert_pos] == '\n') ++insert_pos;
    }
    out.insert(insert_pos, "\n" + typedefs);
  }

  return out;
}

std::string lower_optionals(const std::string &source) {
  static const std::regex opt_re(
      R"(\bstd::optional\s*<\s*([A-Za-z_][\w:]*(?:\s*\*)?)\s*>)");

  // Collect unique optional element types.
  std::set<std::string> seen;
  std::vector<std::string> order;
  for (std::sregex_iterator it(source.begin(), source.end(), opt_re), end;
       it != end; ++it) {
    const std::string t = trim((*it)[1].str());
    if (seen.insert(t).second) order.push_back(t);
  }
  if (order.empty()) return source;

  // Generate typedef structs.
  std::string typedefs;
  std::string out = source;
  for (const std::string &t : order) {
    const std::string c_t = pair_c_type(t); // reuse helper (handles std::string → dpp_string)
    const std::string ident = pair_type_ident(t);
    const std::string sname = "dpp_opt_" + ident;
    typedefs += "typedef struct { " + c_t + " value; int present; } " + sname + ";\n";
    // Replace std::optional<T> occurrences.
    auto re_escape = [](const std::string &s) {
      static const std::regex meta(R"([.^$|()\[\]{}*+?\\])");
      return std::regex_replace(s, meta, "\\$0");
    };
    const std::regex this_opt_re("\\bstd::optional\\s*<\\s*" + re_escape(t) + "\\s*>");
    out = std::regex_replace(out, this_opt_re, sname);
  }

  // std::nullopt → (dpp_opt_T){0, 0} — need to know which type from context.
  // Simple approach: find `return std::nullopt` in an optional-returning function
  // and replace with the right compound literal. Also handle `= std::nullopt`.
  {
    std::map<std::string, std::string> fn_opt_return;
    static const std::regex fn_sig_re(
        R"(\b(dpp_opt_[A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*\([^)]*\)\s*(?:const\s*)?\{)");
    for (std::sregex_iterator it(out.begin(), out.end(), fn_sig_re), end;
         it != end; ++it) {
      fn_opt_return[(*it)[2].str()] = (*it)[1].str();
    }

    std::vector<std::string> lines = split_lines(out);
    std::string cur_opt_type;
    std::size_t fn_brace_depth = 0; // depth inside the optional-returning function body
    static const std::regex ret_null_re(R"(\breturn\s+std::nullopt\s*;)");
    static const std::regex ret_val_re(R"(\breturn\s+(?!std::nullopt\b)([^;]+?)\s*;)");
    for (auto &line : lines) {
      // Count braces in this line to track nesting.
      std::size_t opens = 0, closes = 0;
      for (char c : line) { if (c == '{') ++opens; else if (c == '}') ++closes; }

      std::smatch sm;
      if (std::regex_search(line, sm, fn_sig_re)) {
        cur_opt_type = sm[1].str();
        fn_brace_depth = 0;
      }

      if (!cur_opt_type.empty()) {
        // Rewrite nullopt returns.
        if (std::regex_search(line, ret_null_re)) {
          line = std::regex_replace(line, ret_null_re,
                                    "return (" + cur_opt_type + "){0, 0};");
        } else {
          // Wrap value returns: `return expr;` → `return (dpp_opt_T){expr, 1};`
          std::smatch rm;
          if (std::regex_search(line, rm, ret_val_re)) {
            const std::string expr = trim(rm[1].str());
            if (expr.find(cur_opt_type) == std::string::npos &&
                expr.find("dpp_opt_") == std::string::npos &&
                expr.find("dpp_pair_") == std::string::npos) {
              const std::string ind = leading_indent(line);
              // Replace within the line to preserve surrounding content (e.g. one-liners).
              line = std::regex_replace(line, ret_val_re,
                  "return (" + cur_opt_type + "){" + expr + ", 1};");
            }
          }
        }
        // Track brace depth; reset when we exit the function body.
        fn_brace_depth += opens;
        fn_brace_depth = (closes <= fn_brace_depth) ? fn_brace_depth - closes : 0;
        if (fn_brace_depth == 0) cur_opt_type = "";
        continue;
      }

      // Update depth even outside tracked function.
      fn_brace_depth += opens;
      if (closes <= fn_brace_depth) fn_brace_depth -= closes;
      else fn_brace_depth = 0;
    }
    out = join_lines(lines);
  }

  // `.has_value()` → `.present`  and  `.value()` → `.value`
  out = std::regex_replace(out, std::regex(R"(\.has_value\s*\(\s*\))"), ".present");
  out = std::regex_replace(out, std::regex(R"(\.value\s*\(\s*\))"), ".value");
  // `*opt` dereferencing → `(opt).value`
  static const std::regex deref_re(R"(\*\b(dpp_opt_[A-Za-z_]\w*)\b)");
  out = std::regex_replace(out, deref_re, "($1).value");
  // `std::nullopt` remaining → {0, 0} for assignments.
  out = std::regex_replace(out, std::regex(R"(\bstd::nullopt\b)"), "{0, 0}");
  // Strip `#include <optional>`.
  out = std::regex_replace(out, std::regex(R"(#include\s*<optional>\s*\n)"), "");

  // Insert typedefs after last #include.
  {
    std::size_t insert_pos = 0;
    static const std::regex include_re(R"(#include\s*[<"][^>"]*[>"])");
    for (std::sregex_iterator it(out.begin(), out.end(), include_re), end;
         it != end; ++it) {
      insert_pos = static_cast<std::size_t>((*it).position() + (*it).length());
      while (insert_pos < out.size() && out[insert_pos] == '\n') ++insert_pos;
    }
    out.insert(insert_pos, "\n" + typedefs);
  }

  return out;
}

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
  // Boolean keywords — use `bool` so the stdbool.h include gets triggered downstream.
  if (s == "true" || s == "false") return "bool";

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
  // Pre-scan: build map of function name → return type for known-return-type functions.
  // This allows `auto x = fn(args)` to be resolved when fn has a known non-auto return.
  std::map<std::string, std::string> fn_return_types;
  {
    // Match `ReturnType fn_name(params)` at file/function scope.
    static const std::regex fn_sig_re(
        R"(^([A-Za-z_]\w*(?:\s*\*)?)\s+([A-Za-z_]\w*)\s*\([^)]*\)\s*(?:const\s*)?\{)");
    std::istringstream pre(source);
    std::string pl;
    while (std::getline(pre, pl)) {
      std::smatch sm;
      if (std::regex_search(pl, sm, fn_sig_re)) {
        fn_return_types[sm[2].str()] = sm[1].str();
      }
    }
  }

  static const std::regex auto_re(
      R"(^(\s*)(const\s+)?auto\s+([A-Za-z_]\w*)\s*=\s*(.+)\s*;\s*$)");
  // Match a function-call initializer: fn_name(...) or fn_name_hash(...)
  static const std::regex fn_call_init_re(R"(^([A-Za-z_]\w*)\s*\()");

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
      std::string inferred = infer_auto_type(init);
      if (inferred.empty()) {
        // Try function-call return type lookup.
        std::smatch cm;
        if (std::regex_search(init, cm, fn_call_init_re)) {
          const auto it = fn_return_types.find(cm[1].str());
          if (it != fn_return_types.end()) inferred = it->second;
        }
      }
      if (!inferred.empty()) {
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
  // Strip or convert C++ compat includes.
  static const char *const compat_includes[] = {
      "#include <cstddef>",  "#include <cstdlib>",   "#include <cstring>",
      "#include <climits>",  "#include <utility>",
      "#include <type_traits>", "#include <functional>", "#include <numeric>",
      "#include <stdexcept>", nullptr};

  std::string out;
  {
    std::istringstream in(source);
    std::ostringstream oss;
    std::string ln;
    while (std::getline(in, ln)) {
      const std::string s = trim(ln);
      if (s == "#include <cmath>") {
        oss << "#include <math.h>\n";
        continue;
      }
      bool skip = false;
      for (int i = 0; compat_includes[i]; ++i) {
        if (s == compat_includes[i]) { skip = true; break; }
      }
      if (!skip) oss << ln << '\n';
    }
    out = oss.str();
  }
  // Lower std:: math functions to their C equivalents.
  out = std::regex_replace(out, std::regex(R"(\bstd::sqrt\s*\()"), "sqrt(");
  out = std::regex_replace(out, std::regex(R"(\bstd::pow\s*\()"), "pow(");
  out = std::regex_replace(out, std::regex(R"(\bstd::fabs\s*\()"), "fabs(");
  out = std::regex_replace(out, std::regex(R"(\bstd::ceil\s*\()"), "ceil(");
  out = std::regex_replace(out, std::regex(R"(\bstd::floor\s*\()"), "floor(");
  out = std::regex_replace(out, std::regex(R"(\bstd::round\s*\()"), "round(");
  out = std::regex_replace(out, std::regex(R"(\bstd::abs\s*\()"), "abs(");
  out = std::regex_replace(out, std::regex(R"(\bstd::sin\s*\()"), "sin(");
  out = std::regex_replace(out, std::regex(R"(\bstd::cos\s*\()"), "cos(");
  out = std::regex_replace(out, std::regex(R"(\bstd::log\s*\()"), "log(");
  out = std::regex_replace(out, std::regex(R"(\bstd::log2\s*\()"), "log2(");
  out = std::regex_replace(out, std::regex(R"(\bstd::log10\s*\()"), "log10(");
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
  out = std::regex_replace(out, std::regex(R"(\bstd::size_t\b)"), "size_t");
  out = std::regex_replace(out, std::regex(R"(\bstd::to_string\s*\()"), "dpp_to_string(");
  // Convert "using Alias = Type;" → "typedef Type Alias;"
  out = std::regex_replace(out,
      std::regex(R"(^\s*using\s+([A-Za-z_]\w*)\s*=\s*(.+?)\s*;\s*$)", std::regex::multiline),
      "typedef $2 $1;");
  out = std::regex_replace(out, std::regex(R"(\bnullptr\b)"), "NULL");
  out = std::regex_replace(out, std::regex(R"(\bstd::string::npos\b)"), "((size_t)-1)");
  out = std::regex_replace(out, std::regex(R"(\bconstexpr\s+)"), "const ");
  out = std::regex_replace(out, std::regex(R"(\bstd::string\b)"), "dpp_string");
  out = std::regex_replace(
      out, std::regex(R"(\bconst\s+dpp_string\s*&\s*([A-Za-z_]\w*))"), "const char *$1");
  // Convert `const T &name` → `const T *name` except in range-for loop headers
  // (where `name` is followed by whitespace then `:` — range-for syntax).
  out = std::regex_replace(
      out, std::regex(R"(\bconst\s+([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*)(?!\s*:))"),
      "const $1 *$2");
  out = std::regex_replace(
      out, std::regex(R"(\b([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*)(?!\s*:))"), "$1 *$2");
  out = std::regex_replace(out, std::regex(R"(static_cast\s*<\s*([^>]+)\s*>\s*\(([^;]*)\))"),
                           "(($1)($2))");
  // Only lower simple-identifier `.size()` — skip dotted paths like b.result.size()
  // (those are lowered per-type by lower_strings / lower_vectors before this pass).
  // Use (^|[^.]) to exclude identifiers preceded by a dot.
  out = std::regex_replace(out,
      std::regex(R"((^|[^.A-Za-z_0-9])([A-Za-z_]\w*)\.size\s*\(\s*\))",
                 std::regex::multiline),
      "$1dpp_vector_size(&$2)");
  return out;
}

} // namespace dpp::convert
