#include "dpp/convert/string.h"
#include "cleanup.h"
#include "dpp/string_utils.h"

#include <algorithm>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

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
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.empty\s*\(\s*\))"),
                              "(dpp_string_size(&" + name + ") == 0)");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.size\s*\(\s*\))"),
                              "dpp_string_size(&" + name + ")");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.c_str\s*\(\s*\))"),
                              "dpp_string_c_str(&" + name + ")");
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
  return lower_string_exprs(stripped, strings);
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

} // namespace

StringResult lower_strings(const std::string &source) {
  StringResult result;
  std::vector<ScopedString> strings;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;
  bool inside_record = false;

  auto update_scope = [&](const std::string &line) {
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
  };

  for (const std::string &line : split_lines(source)) {
    const std::size_t before_depth = brace_depth;
    const std::string stripped = trim(line);
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
      out.push_back(indent + "dpp_string " + name + ";");
      out.push_back(indent + "dpp_string_init_cstr(&" + name + ", " +
                    lower_cstr_expr(match[4].str(), live) + ");");
      update_scope(line);
      continue;
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

    std::string lowered = line;
    const std::map<std::string, bool> live = live_strings(strings);
    for (const auto &entry : live) {
      const std::string &name = entry.first;
      const std::regex assign_re("^(\\s*)" + name + R"(\s*=\s*(.+)\s*;\s*$)");
      if (std::regex_match(lowered, match, assign_re)) {
        lowered = match[1].str() + "dpp_string_assign_cstr(&" + name + ", " +
                  lower_cstr_expr(match[2].str(), live) + ");";
      }
      const std::regex append_assign_re("^(\\s*)" + name + R"(\s*\+=\s*(.+)\s*;\s*$)");
      if (std::regex_match(lowered, match, append_assign_re)) {
        lowered = match[1].str() + "dpp_string_append_cstr(&" + name + ", " +
                  lower_cstr_expr(match[2].str(), live) + ");";
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

    lowered = lower_string_exprs(lowered, live);
    const std::string lowered_stripped = trim(lowered);
    if (!strings.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      const std::vector<std::string> return_lines = append_cleanup_before_return(
          lowered, "dpp_string_return_value", destroy_lines(indent, strings));
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
