#include "dpp/convert/string.h"
#include "cleanup.h"
#include "dpp/string_utils.h"

#include <algorithm>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

// Split expr on top-level '+' (ignores '+' inside parens or string literals)
static std::vector<std::string> split_string_concat(const std::string &expr) {
  std::vector<std::string> parts;
  std::string cur;
  int depth = 0;
  bool in_str = false;
  char delim = 0;
  for (std::size_t i = 0; i < expr.size(); ++i) {
    const char c = expr[i];
    if (in_str) {
      cur += c;
      if (c == '\\' && i + 1 < expr.size()) {
        cur += expr[++i];
      } else if (c == delim) {
        in_str = false;
      }
    } else if (c == '"' || c == '\'') {
      in_str = true;
      delim = c;
      cur += c;
    } else if (c == '(') {
      ++depth;
      cur += c;
    } else if (c == ')') {
      --depth;
      cur += c;
    } else if (c == '+' && depth == 0) {
      parts.push_back(trim(cur));
      cur.clear();
    } else {
      cur += c;
    }
  }
  parts.push_back(trim(cur));
  return parts;
}

// Returns {c_expr, is_value_return} for one token in a concat chain
static std::pair<std::string, bool>
classify_concat_part(const std::string &raw, const std::map<std::string, bool> &live,
                     const std::set<std::string> &string_fns) {
  const std::string p = trim(raw);
  // String literal
  if (!p.empty() && p.front() == '"') return {p, false};
  // to_string → dpp_to_string (value-return)
  if (p.find("std::to_string(") != std::string::npos ||
      p.rfind("to_string(", 0) == 0) {
    const std::string lowered =
        std::regex_replace(p, std::regex(R"((?:std::)?to_string\s*\()"), "dpp_to_string(");
    return {lowered, true};
  }
  // Known value-return patterns
  if (p.find("dpp_to_string(") != std::string::npos ||
      p.find("dpp_string_from_") != std::string::npos ||
      p.find("dpp_string_substr(") != std::string::npos) {
    return {p, true};
  }
  // String-returning function call
  std::smatch fm;
  if (std::regex_match(p, fm, std::regex(R"(([A-Za-z_]\w*)\s*\(.*\))"))) {
    if (string_fns.count(fm[1].str()) > 0) return {p, true};
  }
  // Tracked string var or other expression → lower to c_str
  if (live.count(p)) return {"dpp_string_c_str(&" + p + ")", false};
  return {p, false};
}

struct ScopedString {
  std::string name;
  std::size_t depth = 0;
};

std::string compare_expr(const std::string &left, const std::string &op,
                         const std::string &right) {
  return "(" + left + " " + op + " " + right + ")";
}

std::string lower_ordering_exprs(std::string line, const std::string &name,
                                 const std::string &other_name) {
  const std::vector<std::pair<std::string, std::string>> ops = {
      {"<=", "<="}, {">=", ">="}, {"<", "<"}, {">", ">"}};
  for (const auto &op : ops) {
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\s*)" + op.first + R"(\s*)" + other_name + R"(\b)"),
        compare_expr("dpp_string_compare(&" + name + ", &" + other_name + ")", op.second, "0"));
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\s*)" + op.first + R"(\s*("([^"\\]|\\.)*"))"),
        compare_expr("dpp_string_compare_cstr(&" + name + ", $1)", op.second, "0"));
  }
  line = std::regex_replace(
      line, std::regex(R"(("([^"\\]|\\.)*")\s*<=\s*)" + name + R"(\b)"),
      compare_expr("dpp_string_compare_cstr(&" + name + ", $1)", ">=", "0"));
  line = std::regex_replace(
      line, std::regex(R"(("([^"\\]|\\.)*")\s*>=\s*)" + name + R"(\b)"),
      compare_expr("dpp_string_compare_cstr(&" + name + ", $1)", "<=", "0"));
  line = std::regex_replace(
      line, std::regex(R"(("([^"\\]|\\.)*")\s*<\s*)" + name + R"(\b)"),
      compare_expr("dpp_string_compare_cstr(&" + name + ", $1)", ">", "0"));
  line = std::regex_replace(
      line, std::regex(R"(("([^"\\]|\\.)*")\s*>\s*)" + name + R"(\b)"),
      compare_expr("dpp_string_compare_cstr(&" + name + ", $1)", "<", "0"));
  return line;
}

std::string lower_string_exprs(std::string line, const std::map<std::string, bool> &strings) {
  for (const auto &entry : strings) {
    const std::string &name = entry.first;
    for (const auto &other : strings) {
      const std::string &other_name = other.first;
      line = std::regex_replace(
          line, std::regex("\\b" + name + R"(\s*==\s*)" + other_name + R"(\b)"),
          "dpp_string_equal(&" + name + ", &" + other_name + ")");
      line = std::regex_replace(
          line, std::regex("\\b" + name + R"(\s*!=\s*)" + other_name + R"(\b)"),
          "(!dpp_string_equal(&" + name + ", &" + other_name + "))");
      line = lower_ordering_exprs(line, name, other_name);
    }
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\s*==\s*("([^"\\]|\\.)*"))"),
        "dpp_string_equal_cstr(&" + name + ", $1)");
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\s*!=\s*("([^"\\]|\\.)*"))"),
        "(!dpp_string_equal_cstr(&" + name + ", $1))");
    line = std::regex_replace(
        line, std::regex(R"(("([^"\\]|\\.)*")\s*==\s*)" + name + R"(\b)"),
        "dpp_string_equal_cstr(&" + name + ", $1)");
    line = std::regex_replace(
        line, std::regex(R"(("([^"\\]|\\.)*")\s*!=\s*)" + name + R"(\b)"),
        "(!dpp_string_equal_cstr(&" + name + ", $1))");
    line = lower_ordering_exprs(line, name, name);
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.push_back\s*\(\s*(.+)\s*\)\s*;)"),
                              "dpp_string_push_back(&" + name + ", $1);");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\s*\[\s*([^\]]+)\s*\])"),
                              "dpp_string_c_str(&" + name + ")[$1]");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.at\s*\(\s*([^)]+)\s*\))"),
                              "dpp_string_c_str(&" + name + ")[$1]");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.empty\s*\(\s*\))"),
                              "(dpp_string_size(&" + name + ") == 0)");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.size\s*\(\s*\))"),
                              "dpp_string_size(&" + name + ")");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.c_str\s*\(\s*\))"),
                              "dpp_string_c_str(&" + name + ")");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.data\s*\(\s*\))"),
                              "dpp_string_c_str(&" + name + ")");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.length\s*\(\s*\))"),
                              "dpp_string_size(&" + name + ")");
    // find with position argument before plain find
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\.find\s*\(\s*((?:"[^"]*"|[^,)]+))\s*,\s*([^)]+)\s*\))"),
        "dpp_string_find_cstr_from(&" + name + ", $1, $2)");
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\.find\s*\(\s*((?:"[^"]*"|[^)]+))\s*\))"),
        "dpp_string_find_cstr(&" + name + ", $1)");
    // substr(pos, len) → dpp_string_substr — returns dpp_string by value
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\.substr\s*\(\s*([^,)]+)\s*,\s*([^)]+)\s*\))"),
        "dpp_string_substr(&" + name + ", $1, $2)");
    line = std::regex_replace(line, std::regex(R"(<<\s*)" + name + R"(\b(?!\s*\[))"),
                              "<< dpp_string_c_str(&" + name + ")");
  }
  return line;
}

std::string lower_cstr_expr(const std::string &expr, const std::map<std::string, bool> &strings) {
  const std::string stripped = trim(expr);
  if (std::regex_match(stripped, std::regex(R"("([^"\\]|\\.)*")"))) {
    return stripped;
  }
  for (const auto &entry : strings) {
    if (stripped == entry.first) {
      return "dpp_string_c_str(&" + stripped + ")";
    }
  }
  std::string result = lower_string_exprs(stripped, strings);
  // Also wrap dpp_string vars that appear as function call arguments inside the expression.
  for (const auto &entry : strings) {
    const std::string &sname = entry.first;
    result = std::regex_replace(result,
        std::regex("([,(]\\s*)\\b" + sname + "\\b(?!\\s*[.\\[&])"),
        "$1dpp_string_c_str(&" + sname + ")");
  }
  return result;
}

std::map<std::string, bool> live_strings(const std::vector<ScopedString> &strings) {
  std::map<std::string, bool> live;
  for (const ScopedString &string_var : strings) {
    live[string_var.name] = true;
  }
  return live;
}

std::vector<std::string> destroy_lines(const std::string &indent,
                                       const std::vector<ScopedString> &strings) {
  std::vector<std::string> lines;
  for (auto it = strings.rbegin(); it != strings.rend(); ++it) {
    lines.push_back(indent + "dpp_string_destroy(&" + it->name + ");");
  }
  return lines;
}

std::vector<std::string> close_scope_lines(const std::string &indent,
                                           std::vector<ScopedString> &strings,
                                           std::size_t remaining_depth) {
  std::vector<ScopedString> closing;
  for (const ScopedString &string_var : strings) {
    if (string_var.depth > remaining_depth) {
      closing.push_back(string_var);
    }
  }

  strings.erase(std::remove_if(strings.begin(), strings.end(),
                               [remaining_depth](const ScopedString &string_var) {
                                 return string_var.depth > remaining_depth;
                               }),
                strings.end());
  return destroy_lines(indent, closing);
}

std::vector<std::string> peek_destroy_lines(const std::string &indent,
                                            const std::vector<ScopedString> &strings,
                                            std::size_t remaining_depth) {
  std::vector<ScopedString> closing;
  for (const ScopedString &string_var : strings) {
    if (string_var.depth > remaining_depth) {
      closing.push_back(string_var);
    }
  }
  return destroy_lines(indent, closing);
}

} // namespace

// Collect all `dpp_string fieldname;` field names from typedef struct / class bodies.
// Used to lower external struct field accesses like `obj.fieldname.c_str()`.
static std::set<std::string> find_dpp_string_field_names(const std::string &source) {
  std::set<std::string> names;
  static const std::regex struct_open_re(R"(^(?:typedef\s+)?(?:struct|class)\b.*\{)");
  // Match both pre-lowering (std::string / string) and post-lowering (dpp_string) forms.
  static const std::regex field_re(R"(^\s*(?:(?:std::)?string|dpp_string)\s+([A-Za-z_]\w*)\s*;)");
  bool inside_struct = false;
  int depth = 0;
  for (const std::string &line : split_lines(source)) {
    const std::string s = trim(line);
    if (!inside_struct) {
      if (std::regex_search(s, struct_open_re)) {
        inside_struct = true;
        depth = 1;
      }
    } else {
      for (char c : s) {
        if (c == '{') ++depth;
        else if (c == '}') { --depth; if (depth <= 0) { inside_struct = false; break; } }
      }
      if (inside_struct) {
        std::smatch m;
        if (std::regex_search(s, m, field_re)) names.insert(m[1].str());
      }
    }
  }
  return names;
}

// Lower external struct-field string method calls:
// obj.fname.c_str() → dpp_string_c_str(&obj.fname), etc.
// Handles dotted paths (b.sub.fname) via greedy-backtrack in the path capture group.
static std::string lower_string_field_exprs(std::string line,
                                             const std::set<std::string> &field_names) {
  for (const std::string &fname : field_names) {
    const std::string path = R"(([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*))";
    const std::string dot_fname = R"(\.)" + fname;
    line = std::regex_replace(line,
        std::regex(path + dot_fname + R"(\.c_str\s*\(\s*\))"),
        "dpp_string_c_str(&$1." + fname + ")");
    line = std::regex_replace(line,
        std::regex(path + dot_fname + R"(\.data\s*\(\s*\))"),
        "dpp_string_c_str(&$1." + fname + ")");
    line = std::regex_replace(line,
        std::regex(path + dot_fname + R"(\.size\s*\(\s*\))"),
        "dpp_string_size(&$1." + fname + ")");
    line = std::regex_replace(line,
        std::regex(path + dot_fname + R"(\.length\s*\(\s*\))"),
        "dpp_string_size(&$1." + fname + ")");
    line = std::regex_replace(line,
        std::regex(path + dot_fname + R"(\.empty\s*\(\s*\))"),
        "(dpp_string_size(&$1." + fname + ") == 0)");
    // << obj.fname → << dpp_string_c_str(&obj.fname)
    line = std::regex_replace(line,
        std::regex(R"(<<\s*)" + path + dot_fname + R"(\b(?!\s*[.(\[]))"),
        "<< dpp_string_c_str(&$1." + fname + ")");
    // == "literal" and != "literal"
    line = std::regex_replace(line,
        std::regex(path + dot_fname + R"(\s*==\s*("(?:[^"\\]|\\.)*"))"),
        "dpp_string_equal_cstr(&$1." + fname + ", $2)");
    line = std::regex_replace(line,
        std::regex(path + dot_fname + R"(\s*!=\s*("(?:[^"\\]|\\.)*"))"),
        "(!dpp_string_equal_cstr(&$1." + fname + ", $2))");
  }
  return line;
}

// Collect names of functions that return std::string / string (pre-scan).
static std::set<std::string> find_string_returning_fns(const std::string &source) {
  std::set<std::string> names;
  static const std::regex fn_re(
      R"(^(?:(?:static\s+(?:inline\s+)?)?(?:std::)?string|dpp_string)\s+([A-Za-z_]\w*)\s*\()");
  for (const std::string &line : split_lines(source)) {
    std::smatch m;
    const std::string s = trim(line);
    if (std::regex_search(s, m, fn_re)) {
      names.insert(m[1].str());
    }
  }
  return names;
}

// Pre-scan for functions that have string-like parameters.
// Returns: fn_name → list of param names that should be treated as const char *.
// Includes: functions with `const std::string &` params (pre-lowering), and
// functions with `const char *` params that return dpp_string (post-lowering).
static std::map<std::string, std::vector<std::string>>
find_string_param_fns(const std::string &source,
                      const std::set<std::string> &string_fns) {
  std::map<std::string, std::vector<std::string>> result;
  static const std::regex fn_sig_re(
      R"(^(?:[A-Za-z_][\w:*]*\s+)+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:const\s*)?(?:\{|;))");
  for (const std::string &line : split_lines(source)) {
    std::smatch m;
    const std::string s = trim(line);
    if (!std::regex_search(s, m, fn_sig_re)) continue;
    const std::string fn_name = m[1].str();
    const std::string params_raw = m[2].str();
    const bool returns_string = string_fns.count(fn_name) > 0;
    // Split params on commas (top-level only).
    std::vector<std::string> params;
    {
      std::string cur;
      int depth = 0;
      for (char c : params_raw) {
        if (c == '(' || c == '<' || c == '[') { ++depth; cur += c; }
        else if (c == ')' || c == '>' || c == ']') { --depth; cur += c; }
        else if (c == ',' && depth == 0) {
          const std::string t = trim(cur);
          if (!t.empty()) params.push_back(t);
          cur.clear();
        } else cur += c;
      }
      const std::string t = trim(cur);
      if (!t.empty()) params.push_back(t);
    }
    std::vector<std::string> str_params;
    for (const std::string &p : params) {
      const bool is_string_param = p.find("string") != std::string::npos;
      // After lower_free_functions, string params become `const char *` in
      // dpp_string-returning functions. Track those too.
      const bool is_cstr_param =
          returns_string &&
          (p.find("const char *") != std::string::npos ||
           p.find("const char*") != std::string::npos);
      if (!is_string_param && !is_cstr_param) continue;
      // Extract param name: last identifier.
      std::size_t i = p.size();
      while (i > 0 && (std::isalnum(static_cast<unsigned char>(p[i-1])) || p[i-1] == '_')) --i;
      if (i < p.size()) str_params.push_back(p.substr(i));
    }
    if (!str_params.empty()) result[fn_name] = str_params;
  }
  return result;
}

StringResult lower_strings(const std::string &source) {
  StringResult result;
  const std::set<std::string> string_field_names = find_dpp_string_field_names(source);
  const std::set<std::string> string_fns = find_string_returning_fns(source);
  const std::map<std::string, std::vector<std::string>> str_param_fns =
      find_string_param_fns(source, string_fns);
  // Whether the current top-level function returns dpp_string (needs return lowering).
  bool inside_string_ret_fn = false;
  // Active cstr param names for the current function scope.
  std::set<std::string> cstr_params;
  std::vector<ScopedString> strings;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;
  bool inside_record = false;
  std::vector<std::size_t> loop_depths;

  auto update_scope = [&](const std::string &line) {
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    while (!loop_depths.empty() && brace_depth <= loop_depths.back()) {
      loop_depths.pop_back();
    }
  };

  for (const std::string &line : split_lines(source)) {
    const std::size_t before_depth = brace_depth;
    const std::string stripped = trim(line);
    if (is_loop_kw(stripped, "for") || is_loop_kw(stripped, "while") ||
        is_loop_kw(stripped, "do")) {
      loop_depths.push_back(before_depth);
    }
    const bool starts_record =
        std::regex_search(stripped, std::regex(R"(^(?:typedef\s+)?(?:struct|class)\b.*\{\s*$)"));
    if (starts_record) {
      inside_record = true;
    }
    if (stripped == "#include <string>") {
      result.used_string = true;
      update_scope(line);
      continue;
    }

    // When entering a new top-level function, update cstr_params for that function.
    if (brace_depth == 0 && stripped.find('(') != std::string::npos &&
        stripped.back() == '{') {
      // Find the function name from the signature line.
      static const std::regex fn_entry_re(R"(\b([A-Za-z_]\w*)\s*\()");
      std::smatch fem;
      cstr_params.clear();
      inside_string_ret_fn = false;
      if (std::regex_search(stripped, fem, fn_entry_re)) {
        const std::string fn_name = fem[1].str();
        inside_string_ret_fn = string_fns.count(fn_name) > 0;
        const auto it = str_param_fns.find(fn_name);
        if (it != str_param_fns.end()) {
          for (const std::string &pname : it->second) cstr_params.insert(pname);
        }
      }
    }
    if (brace_depth == 1 && stripped == "}") {
      cstr_params.clear();
      inside_string_ret_fn = false;
    }

    std::smatch match;
    static const std::regex default_decl_re(
        R"(^(\s*)(?:std::)?string\s+([A-Za-z_]\w*)\s*;\s*$)");
    if (std::regex_match(line, match, default_decl_re)) {
      result.used_string = true;
      const std::string indent = match[1].str();
      const std::string name = match[2].str();
      if (inside_record) {
        out.push_back(indent + "dpp_string " + name + ";");
        update_scope(line);
        continue;
      }
      strings.push_back({name, before_depth});
      out.push_back(indent + "dpp_string " + name + ";");
      out.push_back(indent + "dpp_string_init(&" + name + ");");
      update_scope(line);
      continue;
    }

    static const std::regex copy_decl_re(
        R"(^(\s*)(const\s+)?(?:std::)?string\s+([A-Za-z_]\w*)\s*=\s*(.+)\s*;\s*$)");
    if (std::regex_match(line, match, copy_decl_re)) {
      result.used_string = true;
      const std::string indent = match[1].str();
      const std::string name = match[3].str();
      if (match[2].matched) {
        out.push_back(indent + "const char *" + name + " = " + match[4].str() + ";");
        update_scope(line);
        continue;
      }
      strings.push_back({name, before_depth});
      const std::map<std::string, bool> live = live_strings(strings);
      const std::string raw_rhs = trim(match[4].str());

      // String concatenation: "a" + b + to_string(n) etc.
      {
        const std::vector<std::string> concat_parts = split_string_concat(raw_rhs);
        if (concat_parts.size() > 1) {
          auto [first_expr, first_is_val] =
              classify_concat_part(concat_parts[0], live, string_fns);
          if (first_is_val) {
            out.push_back(indent + "dpp_string " + name + " = " + first_expr + ";");
          } else {
            out.push_back(indent + "dpp_string " + name + ";");
            out.push_back(indent + "dpp_string_init_cstr(&" + name + ", " + first_expr + ");");
          }
          for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
            auto [expr, is_val] = classify_concat_part(concat_parts[pi], live, string_fns);
            if (is_val) {
              out.push_back(indent + "dpp_string_append_dpp(&" + name + ", " + expr + ");");
            } else {
              out.push_back(indent + "dpp_string_append_cstr(&" + name + ", " + expr + ");");
            }
          }
          update_scope(line);
          continue;
        }
      }

      // Single RHS: map std::to_string(x) → dpp_to_string(x) when it's the entire RHS
      const bool is_to_string =
          (raw_rhs.find("std::to_string(") != std::string::npos ||
           raw_rhs.rfind("to_string(", 0) == 0) &&
          raw_rhs.find('-') == std::string::npos;
      const std::string rhs = is_to_string
          ? std::regex_replace(raw_rhs, std::regex(R"((?:std::)?to_string\s*\()"),
                               "dpp_to_string(")
          : lower_cstr_expr(raw_rhs, live);
      // Detect RHS that returns dpp_string by value (direct struct init, not init_cstr)
      const bool rhs_returns_string = rhs.find("dpp_string_substr(") != std::string::npos ||
          rhs.find("dpp_to_string(") != std::string::npos ||
          rhs.find("dpp_string_from_") != std::string::npos ||
          [&]() {
            const std::string rhs_trimmed = trim(rhs);
            std::smatch fm;
            if (std::regex_match(rhs_trimmed, fm,
                                 std::regex(R"(([A-Za-z_]\w*)\s*\(.*\))"))) {
              return string_fns.count(fm[1].str()) > 0;
            }
            return false;
          }();
      if (rhs_returns_string) {
        out.push_back(indent + "dpp_string " + name + " = " + rhs + ";");
      } else {
        out.push_back(indent + "dpp_string " + name + ";");
        out.push_back(indent + "dpp_string_init_cstr(&" + name + ", " + rhs + ");");
      }
      update_scope(line);
      continue;
    }

    // Fill constructor: std::string name(count, char_literal)
    {
      static const std::regex fill_ctor_re(
          R"(^(\s*)(?:std::)?string\s+([A-Za-z_]\w*)\s*\(\s*(\S+)\s*,\s*('.')\s*\)\s*;\s*$)");
      if (std::regex_match(line, match, fill_ctor_re)) {
        result.used_string = true;
        const std::string indent = match[1].str();
        const std::string name = match[2].str();
        strings.push_back({name, before_depth});
        out.push_back(indent + "dpp_string " + name + ";");
        out.push_back(indent + "dpp_string_init_fill(&" + name + ", " +
                      match[3].str() + ", " + match[4].str() + ");");
        update_scope(line);
        continue;
      }
    }
    static const std::regex ctor_decl_re(
        R"(^(\s*)(?:std::)?string\s+([A-Za-z_]\w*)\s*\((.+)\)\s*;\s*$)");
    if (std::regex_match(line, match, ctor_decl_re)) {
      result.used_string = true;
      const std::string indent = match[1].str();
      const std::string name = match[2].str();
      strings.push_back({name, before_depth});
      const std::map<std::string, bool> live = live_strings(strings);
      out.push_back(indent + "dpp_string " + name + ";");
      out.push_back(indent + "dpp_string_init_cstr(&" + name + ", " +
                    lower_cstr_expr(match[3].str(), live) + ");");
      update_scope(line);
      continue;
    }

    // Range-for over dpp_string: "for (char c : str) {" → index-based loop
    {
      static const std::regex str_range_for_re(
          R"(^(\s*)for\s*\(\s*char\s+([A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)\s*\)\s*\{\s*$)");
      std::smatch rfm;
      if (std::regex_match(line, rfm, str_range_for_re)) {
        const std::map<std::string, bool> live_check = live_strings(strings);
        const std::string sname = rfm[3].str();
        if (live_check.count(sname)) {
          const std::string ind = rfm[1].str();
          const std::string cvar = rfm[2].str();
          const std::string idx = "_dpp_idx_" + cvar;
          out.push_back(ind + "for (size_t " + idx + " = 0; " + idx + " < dpp_string_size(&" +
                        sname + "); ++" + idx + ") {");
          out.push_back(ind + "  char " + cvar + " = dpp_string_c_str(&" + sname + ")[" + idx + "];");
          loop_depths.push_back(before_depth);
          update_scope(line);
          continue;
        }
      }
    }

    std::string lowered = line;
    const std::map<std::string, bool> live = live_strings(strings);
    for (const auto &entry : live) {
      const std::string &name = entry.first;
      const std::regex assign_re("^(\\s*)" + name + R"(\s*=\s*(.+)\s*;\s*$)");
      if (std::regex_match(lowered, match, assign_re)) {
        const std::string ind = match[1].str();
        const std::string raw_assign = trim(match[2].str());
        const std::vector<std::string> concat_parts = split_string_concat(raw_assign);
        if (concat_parts.size() > 1) {
          // Assign first part, append rest
          auto [first_expr, first_is_val] =
              classify_concat_part(concat_parts[0], live, string_fns);
          if (first_is_val) {
            // Use a temp to avoid aliasing issues when name is in concat_parts
            const std::string tmp = "_dpp_concat_tmp";
            lowered = ind + "{ dpp_string " + tmp + " = " + first_expr + ";";
            for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
              auto [expr, is_val] = classify_concat_part(concat_parts[pi], live, string_fns);
              if (is_val)
                lowered += " dpp_string_append_dpp(&" + tmp + ", " + expr + ");";
              else
                lowered += " dpp_string_append_cstr(&" + tmp + ", " + expr + ");";
            }
            lowered += " dpp_string_assign_cstr(&" + name + ", dpp_string_c_str(&" + tmp + "));"
                       " dpp_string_destroy(&" + tmp + "); }";
          } else {
            lowered = ind + "dpp_string_assign_cstr(&" + name + ", " + first_expr + ");";
            for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
              auto [expr, is_val] = classify_concat_part(concat_parts[pi], live, string_fns);
              if (is_val)
                lowered += "\n" + ind + "dpp_string_append_dpp(&" + name + ", " + expr + ");";
              else
                lowered += "\n" + ind + "dpp_string_append_cstr(&" + name + ", " + expr + ");";
            }
          }
        } else {
          lowered = ind + "dpp_string_assign_cstr(&" + name + ", " +
                    lower_cstr_expr(raw_assign, live) + ");";
        }
      }
      const std::regex append_assign_re("^(\\s*)" + name + R"(\s*\+=\s*(.+)\s*;\s*$)");
      if (std::regex_match(lowered, match, append_assign_re)) {
        auto [expr, is_val] =
            classify_concat_part(trim(match[2].str()), live, string_fns);
        if (is_val) {
          lowered = match[1].str() + "dpp_string_append_dpp(&" + name + ", " + expr + ");";
        } else {
          lowered = match[1].str() + "dpp_string_append_cstr(&" + name + ", " + expr + ");";
        }
      }
      const std::regex append_re("^(\\s*)" + name + R"(\.append\s*\((.+)\)\s*;\s*$)");
      if (std::regex_match(lowered, match, append_re)) {
        lowered = match[1].str() + "dpp_string_append_cstr(&" + name + ", " +
                  lower_cstr_expr(match[2].str(), live) + ");";
      }
      const std::regex clear_re("^(\\s*)" + name + R"(\.clear\s*\(\s*\)\s*;\s*$)");
      if (std::regex_match(lowered, match, clear_re)) {
        lowered = match[1].str() + "dpp_string_assign_cstr(&" + name + ", \"\");";
      }
    }

    // Lower std::stoi / std::stod on dpp_string variables
    for (const auto &entry : live) {
      const std::string &sname = entry.first;
      static const std::regex stoi_re(R"((?:std::)?stoi\s*\(\s*)" + sname + R"(\s*\))");
      static const std::regex stod_re(R"((?:std::)?stod\s*\(\s*)" + sname + R"(\s*\))");
      const std::regex stoi_var_re("(?:std::)?stoi\\s*\\(\\s*" + sname + "\\s*\\)");
      const std::regex stod_var_re("(?:std::)?stod\\s*\\(\\s*" + sname + "\\s*\\)");
      lowered = std::regex_replace(lowered, stoi_var_re,
                                   "atoi(dpp_string_c_str(&" + sname + "))");
      lowered = std::regex_replace(lowered, stod_var_re,
                                   "atof(dpp_string_c_str(&" + sname + "))");
    }
    // Lower cstr param method calls before the blanket dpp_vector_size conversion applies.
    for (const std::string &pname : cstr_params) {
      // name.size() → strlen(name)
      lowered = std::regex_replace(lowered,
          std::regex("\\b" + pname + R"(\.size\s*\(\s*\))"), "strlen(" + pname + ")");
      // name.c_str() → name (already const char *)
      lowered = std::regex_replace(lowered,
          std::regex("\\b" + pname + R"(\.c_str\s*\(\s*\))"), pname);
      // name[i] → name[i] (already works for const char *)
      // name.empty() → (name[0] == '\0')
      lowered = std::regex_replace(lowered,
          std::regex("\\b" + pname + R"(\.empty\s*\(\s*\))"), "(" + pname + "[0] == '\\0')");
    }
    // Lower string comparisons and method calls before the call-site c_str wrapping
    // so that patterns like `label == copy` are handled first (wrapping would break them).
    lowered = lower_string_exprs(lowered, live);
    // Lower external struct-field string accesses (obj.fname.c_str(), obj.fname.size(), etc.)
    if (!string_field_names.empty()) {
      lowered = lower_string_field_exprs(lowered, string_field_names);
    }
    // Wrap live dpp_string variables in c_str when used as plain fn arguments
    // (preceded by '(' or ',' and followed by ')' or ',').
    if (!live.empty()) {
      for (const auto &entry : live) {
        const std::string &sname = entry.first;
        lowered = std::regex_replace(lowered,
            std::regex("([,(]\\s*)\\b" + sname + "\\b(?!\\s*[.\\[&])"),
            "$1dpp_string_c_str(&" + sname + ")");
      }
    }
    const std::string lowered_stripped = trim(lowered);
    // Inside a dpp_string-returning function: lower `return concat_expr;`
    // where the expr uses cstr_params (const char *) and/or string literals.
    if (inside_string_ret_fn && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string ret_expr = lowered_stripped.substr(std::string("return ").size());
      if (!ret_expr.empty() && ret_expr.back() == ';') ret_expr.pop_back();
      ret_expr = trim(ret_expr);
      // Only lower if expression is NOT already a tracked dpp_string or a call
      // that returns dpp_string (those are handled below or are already correct).
      const bool is_tracked_var = std::any_of(strings.begin(), strings.end(),
          [&](const ScopedString &s) { return s.name == ret_expr; });
      const bool is_dpp_str_call =
          ret_expr.find("dpp_string_") == 0 ||
          (string_fns.count([&]() {
            const auto p = ret_expr.find('(');
            return p != std::string::npos ? ret_expr.substr(0, p) : ret_expr;
          }()) > 0);
      if (!is_tracked_var && !is_dpp_str_call) {
        const std::vector<std::string> concat_parts = split_string_concat(ret_expr);
        // Helper: classify a part as a cstr-compatible expression.
        auto classify_ret_part = [&](const std::string &raw) -> std::pair<std::string, bool> {
          const std::string p = trim(raw);
          if (!p.empty() && p.front() == '"') return {p, false};  // string literal
          if (cstr_params.count(p)) return {p, false};             // const char * param
          if (live.count(p)) return {"dpp_string_c_str(&" + p + ")", false};
          if (p.find("dpp_to_string(") != std::string::npos ||
              p.find("dpp_string_from_") != std::string::npos ||
              p.find("dpp_string_substr(") != std::string::npos) return {p, true};
          std::smatch fm;
          if (std::regex_match(p, fm, std::regex(R"(([A-Za-z_]\w*)\s*\(.*\))")) &&
              string_fns.count(fm[1].str())) return {p, true};
          // Member access like self->name or obj.field — treat as dpp_string, wrap in c_str.
          if (p.find("->") != std::string::npos || p.find('.') != std::string::npos)
            return {"dpp_string_c_str(&" + p + ")", false};
          return {p, false};
        };
        if (concat_parts.size() >= 1) {
          const std::string tmp = "_dpp_str_ret";
          auto [first_expr, first_is_val] = classify_ret_part(concat_parts[0]);
          if (first_is_val) {
            out.push_back(indent + "dpp_string " + tmp + " = " + first_expr + ";");
          } else {
            out.push_back(indent + "dpp_string " + tmp + ";");
            out.push_back(indent + "dpp_string_init_cstr(&" + tmp + ", " + first_expr + ");");
          }
          for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
            auto [expr, is_val] = classify_ret_part(concat_parts[pi]);
            if (is_val)
              out.push_back(indent + "dpp_string_append_dpp(&" + tmp + ", " + expr + ");");
            else
              out.push_back(indent + "dpp_string_append_cstr(&" + tmp + ", " + expr + ");");
          }
          out.push_back(indent + "return " + tmp + ";");
          result.used_string = true;
          update_scope(line);
          continue;
        }
      }
    }
    if (!strings.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string ret_expr = lowered_stripped.substr(std::string("return ").size());
      if (!ret_expr.empty() && ret_expr.back() == ';') ret_expr.pop_back();
      ret_expr = trim(ret_expr);
      // If returning a tracked dpp_string variable, destroy all OTHERS then return directly.
      const auto ret_string_it = std::find_if(
          strings.begin(), strings.end(),
          [&](const ScopedString &s) { return s.name == ret_expr; });
      if (ret_string_it != strings.end()) {
        for (auto it = strings.rbegin(); it != strings.rend(); ++it) {
          if (it->name != ret_expr) {
            out.push_back(indent + "dpp_string_destroy(&" + it->name + ");");
          }
        }
        out.push_back(lowered);
        strings.clear();
        update_scope(line);
        continue;
      }
      const std::vector<std::string> cleanup = close_scope_lines(indent, strings, 0);
      const std::vector<std::string> return_lines = append_cleanup_before_return(
          lowered, "dpp_string_return_value", cleanup);
      out.insert(out.end(), return_lines.begin(), return_lines.end());
      update_scope(line);
      continue;
    }

    const std::size_t opens = count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    if (!strings.empty() && closes > opens) {
      const std::size_t remaining_depth =
          closes > before_depth + opens ? std::size_t{0} : before_depth + opens - closes;
      const std::vector<std::string> cleanup =
          close_scope_lines(leading_indent(line), strings, remaining_depth);
      out.insert(out.end(), cleanup.begin(), cleanup.end());
    }
    if (!loop_depths.empty() && !strings.empty() &&
        (lowered_stripped == "break;" || lowered_stripped == "continue;")) {
      const std::vector<std::string> cleanup =
          peek_destroy_lines(leading_indent(lowered), strings, loop_depths.back());
      out.insert(out.end(), cleanup.begin(), cleanup.end());
    }
    out.push_back(lowered);
    update_scope(line);
    if (inside_record && std::regex_match(stripped, std::regex(R"(^}\s*[A-Za-z_]\w*\s*;\s*$)"))) {
      inside_record = false;
    }
  }

  result.source = join_lines(out);
  if (!strings.empty()) {
    result.used_string = true;
  }
  return result;
}

} // namespace dpp::convert
