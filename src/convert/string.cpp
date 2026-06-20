#include "dpp/convert/string.h"
#include "dpp/string_utils.h"

#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

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

std::string destroy_lines(const std::string &indent, const std::map<std::string, bool> &strings) {
  std::ostringstream out;
  for (const auto &entry : strings) {
    out << indent << "dpp_string_destroy(&" << entry.first << ");\n";
  }
  return out.str();
}

} // namespace

StringResult lower_strings(const std::string &source) {
  StringResult result;
  std::map<std::string, bool> strings;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;

  auto update_scope = [&](const std::string &line) {
    const std::size_t before = brace_depth;
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    if (before == 1 && brace_depth == 0) {
      strings.clear();
    }
  };

  for (const std::string &line : split_lines(source)) {
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
      strings[name] = true;
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
      strings[name] = true;
      out.push_back(indent + "dpp_string " + name + ";");
      out.push_back(indent + "dpp_string_init_cstr(&" + name + ", " +
                    lower_cstr_expr(match[3].str(), strings) + ");");
      update_scope(line);
      continue;
    }

    static const std::regex ctor_decl_re(
        R"(^(\s*)(?:std::)?string\s+([A-Za-z_]\w*)\s*\((.+)\)\s*;\s*$)");
    if (std::regex_match(line, match, ctor_decl_re)) {
      result.used_string = true;
      const std::string indent = match[1].str();
      const std::string name = match[2].str();
      strings[name] = true;
      out.push_back(indent + "dpp_string " + name + ";");
      out.push_back(indent + "dpp_string_init_cstr(&" + name + ", " +
                    lower_cstr_expr(match[3].str(), strings) + ");");
      update_scope(line);
      continue;
    }

    std::string lowered = line;
    for (const auto &entry : strings) {
      const std::string &name = entry.first;
      const std::regex assign_re("^(\\s*)" + name + R"(\s*=\s*(.+)\s*;\s*$)");
      if (std::regex_match(lowered, match, assign_re)) {
        lowered = match[1].str() + "dpp_string_assign_cstr(&" + name + ", " +
                  lower_cstr_expr(match[2].str(), strings) + ");";
      }
    }

    lowered = lower_string_exprs(lowered, strings);
    const std::string lowered_stripped = trim(lowered);
    if (!strings.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string expr = lowered_stripped.substr(std::string("return ").size());
      if (!expr.empty() && expr.back() == ';') {
        expr.pop_back();
      }
      out.push_back(indent + "int dpp_string_return_value = " + trim(expr) + ";");
      std::string destroys = destroy_lines(indent, strings);
      destroys = destroys.empty() ? "" : destroys.substr(0, destroys.size() - 1);
      if (!destroys.empty()) {
        for (const std::string &destroy_line : split_lines(destroys)) {
          out.push_back(destroy_line);
        }
      }
      out.push_back(indent + "return dpp_string_return_value;");
      update_scope(line);
      continue;
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
