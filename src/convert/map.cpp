#include "dpp/convert/map.h"

#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

struct MapVar {
  std::string key_type;
  std::string value_type;
  bool unordered = false;
};

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

std::string c_type(const std::string &type) {
  return type == "int" ? "int" : type;
}

std::string key_storage_type(const std::string &type) {
  if (type == "char" || type == "int" || type == "long") {
    return "long";
  }
  return c_type(type);
}

std::string map_type(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map" : "dpp_map";
}

std::string size_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_size" : "dpp_map_size";
}

std::string lookup_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_get_or_insert" : "dpp_map_get_or_insert";
}

std::string destroy_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_destroy" : "dpp_map_destroy";
}

std::string init_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_init" : "dpp_map_init";
}

std::string key_temp(const std::string &expr, const std::string &type) {
  return "&(" + key_storage_type(type) + "){" + trim(expr) + "}";
}

std::string value_expr(const std::string &name, const MapVar &var, const std::string &key) {
  return "(*(" + c_type(var.value_type) + " *)" + lookup_fn(var) + "(&" + name + ", " +
         key_temp(key, var.key_type) + "))";
}

std::string lower_map_exprs(std::string line, const std::map<std::string, MapVar> &maps) {
  for (const auto &entry : maps) {
    const std::string &name = entry.first;
    const MapVar &var = entry.second;
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.size\s*\(\s*\))"),
                              size_fn(var) + "(&" + name + ")");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\s*\[\s*([^\]]+)\s*\])"),
                              value_expr(name, var, "$1"));
  }
  return line;
}

std::vector<std::string> destroy_lines(const std::string &indent,
                                       const std::map<std::string, MapVar> &maps) {
  std::vector<std::string> lines;
  for (const auto &entry : maps) {
    lines.push_back(indent + destroy_fn(entry.second) + "(&" + entry.first + ");");
  }
  return lines;
}

} // namespace

MapResult lower_maps(const std::string &source) {
  MapResult result;
  std::map<std::string, MapVar> maps;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;

  auto update_scope = [&](const std::string &line) {
    const std::size_t before = brace_depth;
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    if (before == 1 && brace_depth == 0) {
      maps.clear();
    }
  };

  for (const std::string &line : split_lines(source)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <map>" || stripped == "#include <unordered_map>") {
      result.used_map = true;
      update_scope(line);
      continue;
    }

    std::smatch match;
    static const std::regex map_decl_re(
        R"(^(\s*)(?:std::)?(unordered_map|map)\s*<\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;\s*$)");
    if (std::regex_match(line, match, map_decl_re)) {
      result.used_map = true;
      const std::string indent = match[1].str();
      const MapVar var{match[3].str(), match[4].str(), match[2].str() == "unordered_map"};
      const std::string name = match[5].str();
      maps[name] = var;
      out.push_back(indent + map_type(var) + " " + name + ";");
      if (var.unordered) {
        out.push_back(indent + init_fn(var) + "(&" + name + ", sizeof(" +
                      key_storage_type(var.key_type) + "), sizeof(" + c_type(var.value_type) +
                      "), dpp_hash_long, dpp_equal_long);");
      } else {
        out.push_back(indent + init_fn(var) + "(&" + name + ", sizeof(" +
                      key_storage_type(var.key_type) + "), sizeof(" + c_type(var.value_type) +
                      "), dpp_compare_long);");
      }
      update_scope(line);
      continue;
    }

    const std::string lowered = lower_map_exprs(line, maps);
    const std::string lowered_stripped = trim(lowered);
    if (!maps.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string expr = lowered_stripped.substr(std::string("return ").size());
      if (!expr.empty() && expr.back() == ';') {
        expr.pop_back();
      }
      out.push_back(indent + "int dpp_map_return_value = " + trim(expr) + ";");
      for (const std::string &destroy : destroy_lines(indent, maps)) {
        out.push_back(destroy);
      }
      out.push_back(indent + "return dpp_map_return_value;");
      update_scope(line);
      continue;
    }

    out.push_back(lowered);
    update_scope(line);
  }

  result.source = join_lines(out);
  return result;
}

} // namespace dpp::convert
