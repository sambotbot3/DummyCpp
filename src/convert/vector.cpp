#include "dpp/convert/vector.h"

#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

std::string trim(const std::string &value) {
  const std::string whitespace = " \t\r\n";
  const std::size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos) {
    return "";
  }
  const std::size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

std::string leading_indent(const std::string &line) {
  const std::size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return line;
  }
  return line.substr(0, first);
}

std::vector<std::string> split_lines(const std::string &source) {
  std::istringstream in(source);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    lines.push_back(line);
  }
  return lines;
}

std::string join_lines(const std::vector<std::string> &lines) {
  std::ostringstream out;
  for (const std::string &line : lines) {
    out << line << '\n';
  }
  return out.str();
}

std::size_t count_char(const std::string &line, char ch) {
  std::size_t count = 0;
  for (const char item : line) {
    if (item == ch) {
      ++count;
    }
  }
  return count;
}

std::string c_type_for_vector_elem(const std::string &elem_type) {
  if (elem_type == "int") {
    return "int";
  }
  return elem_type;
}

std::string lower_push_value(const std::string &expr, const std::string &c_type) {
  const std::string trimmed = trim(expr);
  const std::string aggregate_prefix = c_type + "{";
  if (trimmed.rfind(aggregate_prefix, 0) == 0 && !trimmed.empty() && trimmed.back() == '}') {
    return "(" + c_type + ")" + trimmed.substr(c_type.size());
  }
  if (std::regex_match(trimmed, std::regex(R"([A-Za-z_]\w*)"))) {
    return trimmed;
  }
  return "(" + c_type + "){" + trimmed + "}";
}

std::string lower_vector_exprs(std::string line, const std::map<std::string, std::string> &vectors) {
  for (const auto &entry : vectors) {
    const std::string &name = entry.first;
    const std::string c_type = c_type_for_vector_elem(entry.second);
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\s*\[\s*([^\]]+)\s*\]\.([A-Za-z_]\w*)\s*\(\s*\))"),
        c_type + "_$2((" + c_type + " *)dpp_vector_at(&" + name + ", $1))");
    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(\s*\[\s*([^\]]+)\s*\]\.([A-Za-z_]\w*)\s*\(([^()]*)\))"),
        c_type + "_$2((" + c_type + " *)dpp_vector_at(&" + name + ", $1), $3)");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.size\s*\(\s*\))"),
                              "dpp_vector_size(&" + name + ")");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\s*\[\s*([^\]]+)\s*\])"),
                              "(*(" + c_type + " *)dpp_vector_at(&" + name + ", $1))");
  }
  return line;
}

std::string destroy_lines(const std::string &indent, const std::map<std::string, std::string> &vectors) {
  std::ostringstream out;
  for (const auto &entry : vectors) {
    out << indent << "dpp_vector_destroy(&" << entry.first << ");\n";
  }
  return out.str();
}

} // namespace

VectorResult lower_vectors(const std::string &source) {
  VectorResult result;
  std::map<std::string, std::string> vectors;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;

  auto update_scope = [&](const std::string &line) {
    const std::size_t before = brace_depth;
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    if (before == 1 && brace_depth == 0) {
      vectors.clear();
    }
  };

  for (const std::string &line : split_lines(source)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <vector>") {
      result.used_vector = true;
      update_scope(line);
      continue;
    }

    std::smatch match;
    static const std::regex vector_decl_re(
        R"(^(\s*)(?:std::)?vector\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;\s*$)");
    if (std::regex_match(line, match, vector_decl_re)) {
      result.used_vector = true;
      const std::string indent = match[1].str();
      const std::string elem_type = match[2].str();
      const std::string name = match[3].str();
      vectors[name] = elem_type;
      out.push_back(indent + "dpp_vector " + name + ";");
      out.push_back(indent + "dpp_vector_init(&" + name + ", sizeof(" +
                    c_type_for_vector_elem(elem_type) + "));");
      update_scope(line);
      continue;
    }

    std::string lowered = line;
    for (const auto &entry : vectors) {
      const std::string &name = entry.first;
      const std::string c_type = c_type_for_vector_elem(entry.second);
      const std::regex push_re("^(\\s*)" + name + R"(\.push_back\s*\((.*)\)\s*;\s*$)");
      if (std::regex_match(lowered, match, push_re)) {
        lowered = match[1].str() + "dpp_vector_push_back(&" + name + ", &" +
                  lower_push_value(match[2].str(), c_type) + ");";
      }
    }

    lowered = lower_vector_exprs(lowered, vectors);
    const std::string lowered_stripped = trim(lowered);
    if (!vectors.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string expr = lowered_stripped.substr(std::string("return ").size());
      if (!expr.empty() && expr.back() == ';') {
        expr.pop_back();
      }
      out.push_back(indent + "int dpp_return_value = " + trim(expr) + ";");
      std::string destroys = destroy_lines(indent, vectors);
      destroys = destroys.empty() ? "" : destroys.substr(0, destroys.size() - 1);
      if (!destroys.empty()) {
        for (const std::string &destroy_line : split_lines(destroys)) {
          out.push_back(destroy_line);
        }
      }
      out.push_back(indent + "return dpp_return_value;");
      update_scope(line);
      continue;
    }

    out.push_back(lowered);
    update_scope(line);
  }

  result.source = join_lines(out);
  if (!vectors.empty()) {
    result.used_vector = true;
  }
  return result;
}

} // namespace dpp::convert
