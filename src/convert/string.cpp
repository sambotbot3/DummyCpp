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

std::string lower_string_exprs(std::string line, const std::map<std::string, bool> &strings) {
  for (const auto &entry : strings) {
    const std::string &name = entry.first;
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

  auto update_scope = [&](const std::string &line) {
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
  };

  for (const std::string &line : split_lines(source)) {
    const std::size_t before_depth = brace_depth;
    const std::string stripped = trim(line);
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
      strings.push_back({name, before_depth});
      out.push_back(indent + "dpp_string " + name + ";");
      out.push_back(indent + "dpp_string_init(&" + name + ");");
      update_scope(line);
      continue;
    }

    static const std::regex copy_decl_re(
        R"(^(\s*)(?:std::)?string\s+([A-Za-z_]\w*)\s*=\s*(.+)\s*;\s*$)");
    if (std::regex_match(line, match, copy_decl_re)) {
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
  }

  result.source = join_lines(out);
  if (!strings.empty()) {
    result.used_string = true;
  }
  return result;
}

} // namespace dpp::convert
