#include "dpp/convert/memory.h"
#include "dpp/string_utils.h"

#include <map>
#include <regex>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

struct SmartVar {
  std::string type;
  bool shared = false;
};

bool is_primitive_type(const std::string &type) {
  return type == "bool" || type == "char" || type == "int" || type == "long" ||
         type == "float" || type == "double";
}

std::string destructor_for_type(const std::string &type) {
  return is_primitive_type(type) ? "NULL" : type + "_destroy";
}

std::string pointee_expr(const std::string &name, const SmartVar &var) {
  if (!var.shared) {
    return name;
  }
  return "((" + var.type + " *)dpp_shared_get(&" + name + "))";
}

std::string cleanup_line(const std::string &indent, const std::string &name,
                         const SmartVar &var) {
  if (var.shared) {
    return indent + "dpp_shared_destroy(&" + name + ");";
  }
  return indent + "dpp_unique_destroy(" + name + ", " + destructor_for_type(var.type) + ");";
}

std::vector<std::string> cleanup_lines(const std::string &indent,
                                       const std::map<std::string, SmartVar> &vars,
                                       const std::vector<std::string> &order) {
  std::vector<std::string> lines;
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    const auto found = vars.find(*it);
    if (found != vars.end()) {
      lines.push_back(cleanup_line(indent, found->first, found->second));
    }
  }
  return lines;
}

void remember_var(std::map<std::string, SmartVar> &vars, std::vector<std::string> &order,
                  const std::string &name, const SmartVar &var) {
  if (vars.find(name) == vars.end()) {
    order.push_back(name);
  }
  vars[name] = var;
}

std::string lower_smart_exprs(std::string line, const std::map<std::string, SmartVar> &vars) {
  for (const auto &entry : vars) {
    const std::string &name = entry.first;
    const SmartVar &var = entry.second;
    const std::string ptr = pointee_expr(name, var);

    line = std::regex_replace(
        line, std::regex("\\b" + name + R"(->([A-Za-z_]\w*)\s*\(([^()]*)\))"),
        var.type + "_$1(" + ptr + (std::string("$2").empty() ? "" : ", $2") + ")");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(->([A-Za-z_]\w*)\b)"),
                              ptr + "->$1");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.get\s*\(\s*\))"), ptr);
    line = std::regex_replace(line, std::regex("\\*" + name + R"(\b)"), "*" + ptr);
  }

  line = std::regex_replace(line, std::regex(R"(,\s*\))"), ")");
  return line;
}

std::vector<std::string> lower_heap_init(const std::string &indent, const std::string &kind,
                                         const std::string &type, const std::string &name,
                                         const std::string &args) {
  std::vector<std::string> lines;
  const bool shared = kind == "shared";
  const std::string destroy = destructor_for_type(type);
  if (shared) {
    lines.push_back(indent + "dpp_shared_ptr " + name + ";");
    lines.push_back(indent + type + " *dpp_" + name + "_value = (" + type +
                    " *)dpp_shared_create(&" + name + ", sizeof(" + type + "), " +
                    destroy + ");");
    if (is_primitive_type(type)) {
      lines.push_back(indent + "*dpp_" + name + "_value = " + trim(args) + ";");
    } else {
      lines.push_back(indent + type + "_init(dpp_" + name + "_value" +
                      (trim(args).empty() ? "" : ", " + trim(args)) + ");");
    }
    return lines;
  }

  lines.push_back(indent + type + " *" + name + " = (" + type + " *)dpp_unique_new(sizeof(" +
                  type + "));");
  if (is_primitive_type(type)) {
    lines.push_back(indent + "*" + name + " = " + trim(args) + ";");
  } else {
    lines.push_back(indent + type + "_init(" + name + (trim(args).empty() ? "" : ", " + trim(args)) +
                    ");");
  }
  return lines;
}

} // namespace

MemoryResult lower_memory(const std::string &source) {
  MemoryResult result;
  std::map<std::string, SmartVar> vars;
  std::vector<std::string> order;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;

  for (const std::string &line : split_lines(source)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <memory>") {
      result.used_memory = true;
      continue;
    }

    const std::size_t before_depth = brace_depth;
    std::smatch match;
    static const std::regex heap_decl_re(
        R"(^(\s*)(?:std::)?(unique|shared)_ptr\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*\(\s*new\s+\3\s*\(([^)]*)\)\s*\)\s*;\s*$)");
    if (std::regex_match(line, match, heap_decl_re)) {
      result.used_memory = true;
      const std::string indent = match[1].str();
      const std::string kind = match[2].str();
      const std::string type = match[3].str();
      const std::string name = match[4].str();
      remember_var(vars, order, name, {type, kind == "shared"});
      for (const std::string &lowered : lower_heap_init(indent, kind, type, name, match[5].str())) {
        out.push_back(lowered);
      }
      brace_depth += count_char(line, '{');
      const std::size_t closes = count_char(line, '}');
      brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
      continue;
    }

    static const std::regex shared_copy_re(
        R"(^(\s*)(?:std::)?shared_ptr\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*;\s*$)");
    if (std::regex_match(line, match, shared_copy_re)) {
      result.used_memory = true;
      const std::string indent = match[1].str();
      const std::string type = match[2].str();
      const std::string name = match[3].str();
      const std::string source_name = match[4].str();
      remember_var(vars, order, name, {type, true});
      out.push_back(indent + "dpp_shared_ptr " + name + ";");
      out.push_back(indent + "dpp_shared_copy(&" + name + ", &" + source_name + ");");
      brace_depth += count_char(line, '{');
      const std::size_t closes = count_char(line, '}');
      brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
      continue;
    }

    std::string lowered = lower_smart_exprs(line, vars);
    const std::string lowered_stripped = trim(lowered);
    if (!vars.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string expr = lowered_stripped.substr(std::string("return ").size());
      if (!expr.empty() && expr.back() == ';') {
        expr.pop_back();
      }
      out.push_back(indent + "int dpp_memory_return_value = " + trim(expr) + ";");
      for (const std::string &cleanup : cleanup_lines(indent, vars, order)) {
        out.push_back(cleanup);
      }
      out.push_back(indent + "return dpp_memory_return_value;");
      vars.clear();
      order.clear();
    } else {
      const std::size_t opens = count_char(line, '{');
      const std::size_t closes = count_char(line, '}');
      if (!vars.empty() && before_depth == 1 && closes > opens) {
        for (const std::string &cleanup : cleanup_lines(leading_indent(line), vars, order)) {
          out.push_back(cleanup);
        }
      }
      out.push_back(lowered);
    }

    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    if (before_depth == 1 && brace_depth == 0) {
      vars.clear();
      order.clear();
    }
  }

  result.source = join_lines(out);
  if (!vars.empty()) {
    result.used_memory = true;
  }
  return result;
}

} // namespace dpp::convert
