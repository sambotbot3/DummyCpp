#include "dpp/convert/map.h"
#include "dpp/string_utils.h"

#include <algorithm>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

struct MapVar {
  std::string key_type;
  std::string value_type;
  bool unordered = false;
  bool pointer = false;
};

struct RangeVar {
  std::string entry_name;
  std::string container_name;
  MapVar map;
  std::string index_name;
  std::size_t depth = 0;
};

std::string c_type(const std::string &type) {
  if (type == "std::string" || type == "string") {
    return "dpp_string";
  }
  return type == "int" ? "int" : type;
}

std::string key_storage_type(const std::string &type) {
  if (type == "char" || type == "int" || type == "long") {
    return "long";
  }
  if (type == "std::string" || type == "string") {
    return "const char *";
  }
  return c_type(type);
}

bool is_string_key(const std::string &type) {
  return type == "std::string" || type == "string";
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

std::string key_at_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_key_at" : "dpp_map_key_at";
}

std::string value_at_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_value_at" : "dpp_map_value_at";
}

std::string const_value_at_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_const_value_at" : "dpp_map_const_value_at";
}

std::string destroy_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_destroy" : "dpp_map_destroy";
}

std::string contains_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_contains" : "dpp_map_contains";
}

std::string init_fn(const MapVar &var) {
  return var.unordered ? "dpp_unordered_map_init" : "dpp_map_init";
}

std::string map_access(const std::string &name, const MapVar &var) {
  return var.pointer ? name : "&" + name;
}

std::string key_value_expr(const std::string &expr, const std::string &type) {
  const std::string stripped = trim(expr);
  if (!is_string_key(type)) {
    return stripped;
  }
  if (std::regex_match(stripped, std::regex(R"("([^"\\]|\\.)*")"))) {
    return stripped;
  }
  if (stripped == "key" || stripped.find("dpp_string_c_str(") != std::string::npos ||
      stripped.find(".c_str()") != std::string::npos) {
    return stripped;
  }
  if (std::regex_match(stripped, std::regex(R"([A-Za-z_]\w*)"))) {
    return "dpp_string_c_str(&" + stripped + ")";
  }
  return stripped;
}

std::string key_temp(const std::string &expr, const std::string &type) {
  return "&(" + key_storage_type(type) + "){" + key_value_expr(expr, type) + "}";
}

std::string value_expr(const std::string &name, const MapVar &var, const std::string &key) {
  return "(*(" + c_type(var.value_type) + " *)" + lookup_fn(var) + "(" + map_access(name, var) +
         ", " + key_temp(key, var.key_type) + "))";
}

std::string key_entry_expr(const std::string &container, const MapVar &var,
                           const std::string &index) {
  return "(*(" + key_storage_type(var.key_type) + " *)" + key_at_fn(var) + "(" +
         map_access(container, var) + ", " + index + "))";
}

std::string value_entry_expr(const std::string &container, const MapVar &var,
                             const std::string &index) {
  const std::string access_fn = var.pointer ? const_value_at_fn(var) : value_at_fn(var);
  return "(*(" + c_type(var.value_type) + " *)" + access_fn + "(" + map_access(container, var) +
         ", " + index + "))";
}

std::string value_entry_ptr_expr(const std::string &container, const MapVar &var,
                                 const std::string &index) {
  const std::string access_fn = var.pointer ? const_value_at_fn(var) : value_at_fn(var);
  return "((" + c_type(var.value_type) + " *)" + access_fn + "(" + map_access(container, var) +
         ", " + index + "))";
}

std::string lower_map_exprs(std::string line, const std::map<std::string, MapVar> &maps) {
  for (const auto &entry : maps) {
    const std::string &name = entry.first;
    const MapVar &var = entry.second;
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.size\s*\(\s*\))"),
                              size_fn(var) + "(" + map_access(name, var) + ")");
    // .count(key) and .contains(key) → dpp_map_contains / dpp_unordered_map_contains
    const std::string cfn = contains_fn(var);
    const std::string kst = key_storage_type(var.key_type);
    const std::string access = map_access(name, var);
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.count\s*\(\s*([^)]+)\s*\))"),
                              cfn + "(" + access + ", &(" + kst + "){$1})");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.contains\s*\(\s*([^)]+)\s*\))"),
                              cfn + "(" + access + ", &(" + kst + "){$1})");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\s*\[\s*([^\]]+)\s*\])"),
                              value_expr(name, var, "$1"));
  }
  return line;
}

std::string lower_map_calls(std::string line, const std::map<std::string, MapVar> &maps) {
  for (const auto &entry : maps) {
    const std::string &name = entry.first;
    const MapVar &var = entry.second;
    const std::string arg = var.pointer ? name : "&" + name;
    line = std::regex_replace(line,
                              std::regex(R"(\badd_to_total__string__int\s*\(\s*)" + name +
                                         R"(\s*,\s*([A-Za-z_]\w*)\s*,)"),
                              "add_to_total__string__int(" + arg + ", $1,");
    if (!var.pointer) {
      line = std::regex_replace(
          line, std::regex(R"((\b[A-Za-z_]\w*\s*\(\s*))" + name + R"(\s*(,|\)))"),
          "$1&" + name + "$2");
    }
  }
  return line;
}

std::string lower_range_entry_exprs(std::string line, const std::vector<RangeVar> &ranges) {
  for (const RangeVar &range : ranges) {
    const std::string entry = range.entry_name;
    line = std::regex_replace(
        line, std::regex(R"(&\s*\()" + range.map.value_type + R"(\)\s*\{\s*)" + entry +
                         R"(\.second\s*\})"),
        value_entry_ptr_expr(range.container_name, range.map, range.index_name));
    line = std::regex_replace(
        line, std::regex("\\b([A-Za-z_]\\w*__\\w+)\\s*\\(\\s*" + entry + R"(\.second\s*\))"),
        "$1(" + value_entry_ptr_expr(range.container_name, range.map, range.index_name) + ")");
    line = std::regex_replace(
        line,
        std::regex(R"(\bconst\s+([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*)\s*=\s*)" + entry +
                   R"(\.second\s*;)"),
        "const $1 *$2 = " +
            value_entry_ptr_expr(range.container_name, range.map, range.index_name) + ";");
    line = std::regex_replace(line, std::regex("\\b" + entry + R"(\.first\b)"),
                              key_entry_expr(range.container_name, range.map, range.index_name));
    line = std::regex_replace(line, std::regex("\\b" + entry + R"(\.second\b)"),
                              value_entry_expr(range.container_name, range.map, range.index_name));
    line = std::regex_replace(line, std::regex(R"(\bitem\.)"), "item->");
  }
  return line;
}

std::vector<std::string> split_params(const std::string &params) {
  std::vector<std::string> out;
  for (const std::string &part : split_commas(params)) {
    const std::string trimmed = trim(part);
    if (!trimmed.empty()) {
      out.push_back(trimmed);
    }
  }
  return out;
}

bool map_alias_param(const std::string &param, const std::map<std::string, MapVar> &aliases,
                     std::string *name, MapVar *var) {
  std::smatch match;
  static const std::regex param_re(
      R"(^\s*(?:const\s+)?([A-Za-z_]\w*)\s*([&*])\s*([A-Za-z_]\w*)\s*$)");
  if (!std::regex_match(param, match, param_re)) {
    return false;
  }
  const auto alias = aliases.find(match[1].str());
  if (alias == aliases.end()) {
    return false;
  }
  *name = match[3].str();
  *var = alias->second;
  var->pointer = true;
  return true;
}

void register_function_map_params(const std::string &line,
                                  const std::map<std::string, MapVar> &aliases,
                                  std::map<std::string, MapVar> *maps) {
  std::smatch match;
  static const std::regex function_re(
      R"(^\s*[A-Za-z_][\w:<>\s*&]*\s+([A-Za-z_]\w*)\s*\(([^()]*)\)\s*\{\s*$)");
  if (std::regex_match(line, match, function_re)) {
    for (const std::string &param : split_params(match[2].str())) {
      std::string name;
      MapVar var;
      if (map_alias_param(param, aliases, &name, &var)) {
        (*maps)[name] = var;
      }
    }
  }

  static const std::regex generated_map_fn_re(
      R"(^\s*\w+\s+[A-Za-z_]\w*__string__int\s*\(\s*dpp_map\s*\*\s*([A-Za-z_]\w*)\s*,.*\)\s*\{\s*$)");
  if (std::regex_search(line, match, generated_map_fn_re)) {
    (*maps)[match[1].str()] = MapVar{"std::string", "int", false, true};
  }
}

std::vector<std::string> init_lines(const std::string &indent, const std::string &name,
                                    const MapVar &var) {
  const std::string compare = is_string_key(var.key_type) ? "dpp_compare_cstr" : "dpp_compare_long";
  const std::string hash = is_string_key(var.key_type) ? "dpp_hash_cstr" : "dpp_hash_long";
  const std::string equal = is_string_key(var.key_type) ? "dpp_equal_cstr" : "dpp_equal_long";
  if (var.unordered) {
    return {indent + init_fn(var) + "(&" + name + ", sizeof(" + key_storage_type(var.key_type) +
            "), sizeof(" + c_type(var.value_type) + "), " + hash + ", " + equal + ");"};
  }
  return {indent + init_fn(var) + "(&" + name + ", sizeof(" + key_storage_type(var.key_type) +
          "), sizeof(" + c_type(var.value_type) + "), " + compare + ");"};
}

std::vector<std::string> destroy_lines(const std::string &indent,
                                       const std::map<std::string, MapVar> &maps) {
  std::vector<std::string> lines;
  for (const auto &entry : maps) {
    if (!entry.second.pointer) {
      lines.push_back(indent + destroy_fn(entry.second) + "(&" + entry.first + ");");
    }
  }
  return lines;
}

bool has_owned_maps(const std::map<std::string, MapVar> &maps) {
  for (const auto &entry : maps) {
    if (!entry.second.pointer) {
      return true;
    }
  }
  return false;
}

} // namespace

MapResult lower_maps(const std::string &source) {
  MapResult result;
  std::map<std::string, MapVar> aliases;
  std::map<std::string, MapVar> maps;
  std::vector<RangeVar> ranges;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;

  auto update_scope = [&](const std::string &line) {
    const std::size_t before = brace_depth;
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    if (before == 1 && brace_depth == 0) {
      maps.clear();
      ranges.clear();
    }
    ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
                                [&](const RangeVar &range) { return range.depth > brace_depth; }),
                 ranges.end());
  };

  for (const std::string &line : split_lines(source)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <map>" || stripped == "#include <unordered_map>") {
      result.used_map = true;
      update_scope(line);
      continue;
    }

    std::smatch match;
    static const std::regex map_alias_re(
        R"(^\s*typedef\s+(?:std::)?(unordered_map|map)\s*<\s*(std::string|string|[A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;\s*$)");
    if (std::regex_match(line, match, map_alias_re)) {
      result.used_map = true;
      aliases[match[4].str()] =
          MapVar{match[2].str(), match[3].str(), match[1].str() == "unordered_map", false};
      out.push_back(line);
      update_scope(line);
      continue;
    }

    register_function_map_params(line, aliases, &maps);

    static const std::regex map_decl_re(
        R"(^(\s*)(?:std::)?(unordered_map|map)\s*<\s*(std::string|string|[A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;\s*$)");
    if (std::regex_match(line, match, map_decl_re)) {
      result.used_map = true;
      const std::string indent = match[1].str();
      const MapVar var{match[3].str(), match[4].str(), match[2].str() == "unordered_map", false};
      const std::string name = match[5].str();
      maps[name] = var;
      out.push_back(indent + map_type(var) + " " + name + ";");
      const std::vector<std::string> init = init_lines(indent, name, var);
      out.insert(out.end(), init.begin(), init.end());
      update_scope(line);
      continue;
    }

    static const std::regex alias_decl_re(
        R"(^(\s*)(?:const\s+)?([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*(=.*)?;\s*$)");
    if (std::regex_match(line, match, alias_decl_re)) {
      const auto alias = aliases.find(match[2].str());
      if (alias != aliases.end()) {
        result.used_map = true;
        const std::string indent = match[1].str();
        const std::string name = match[3].str();
        const MapVar var = alias->second;
        maps[name] = var;
        out.push_back(indent + match[2].str() + " " + name + match[4].str() + ";");
        if (line.find('=') == std::string::npos) {
          const std::vector<std::string> init = init_lines(indent, name, var);
          out.insert(out.end(), init.begin(), init.end());
        }
        update_scope(line);
        continue;
      }
    }

    static const std::regex range_for_re(
        R"(^(\s*)for\s*\(\s*const\s+auto\s*&\s*([A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)\s*\)\s*\{\s*$)");
    if (std::regex_match(line, match, range_for_re)) {
      const std::string container = match[3].str();
      const auto map = maps.find(container);
      if (map != maps.end()) {
        result.used_map = true;
        const std::string index = "dpp_map_index_" + match[2].str();
        ranges.push_back({match[2].str(), container, map->second, index, brace_depth + 1});
        out.push_back(match[1].str() + "for (size_t " + index + " = 0; " + index + " < " +
                      size_fn(map->second) + "(" + map_access(container, map->second) + "); ++" +
                      index + ") {");
        update_scope(line);
        continue;
      }
    }

    const std::string lowered =
        lower_map_calls(lower_map_exprs(lower_range_entry_exprs(line, ranges), maps), maps);
    const std::string lowered_stripped = trim(lowered);
    if (!maps.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string expr = lowered_stripped.substr(std::string("return ").size());
      if (!expr.empty() && expr.back() == ';') {
        expr.pop_back();
      }
      if (maps.find(trim(expr)) != maps.end()) {
        out.push_back(lowered);
        update_scope(line);
        continue;
      }
      if (!has_owned_maps(maps)) {
        out.push_back(lowered);
        update_scope(line);
        continue;
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
