#include "dpp/convert/memory.h"
#include "cleanup.h"
#include "dpp/string_utils.h"

#include <algorithm>
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

struct ScopedSmartVar {
  std::string name;
  SmartVar var;
  std::size_t depth = 0;
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
                                       const std::vector<ScopedSmartVar> &vars) {
  std::vector<std::string> lines;
  for (auto it = vars.rbegin(); it != vars.rend(); ++it) {
    lines.push_back(cleanup_line(indent, it->name, it->var));
  }
  return lines;
}

std::map<std::string, SmartVar> live_vars(const std::vector<ScopedSmartVar> &vars) {
  std::map<std::string, SmartVar> live;
  for (const ScopedSmartVar &var : vars) {
    live[var.name] = var.var;
  }
  return live;
}

void remember_var(std::vector<ScopedSmartVar> &vars, const std::string &name, const SmartVar &var,
                  std::size_t depth) {
  vars.push_back({name, var, depth});
}

std::vector<std::string> close_scope_lines(const std::string &indent,
                                           std::vector<ScopedSmartVar> &vars,
                                           std::size_t remaining_depth) {
  std::vector<ScopedSmartVar> closing;
  for (const ScopedSmartVar &var : vars) {
    if (var.depth > remaining_depth) {
      closing.push_back(var);
    }
  }

  vars.erase(std::remove_if(vars.begin(), vars.end(),
                            [remaining_depth](const ScopedSmartVar &var) {
                              return var.depth > remaining_depth;
                            }),
             vars.end());
  return cleanup_lines(indent, closing);
}

// Like close_scope_lines but does NOT remove vars — for break/continue paths
// where the normal scope-exit at } still needs to run on non-break iterations.
std::vector<std::string> peek_cleanup_lines(const std::string &indent,
                                            const std::vector<ScopedSmartVar> &vars,
                                            std::size_t remaining_depth) {
  std::vector<ScopedSmartVar> closing;
  for (const ScopedSmartVar &var : vars) {
    if (var.depth > remaining_depth) {
      closing.push_back(var);
    }
  }
  return cleanup_lines(indent, closing);
}

bool is_loop_kw(const std::string &s, const std::string &kw) {
  if (s.size() < kw.size() || s.compare(0, kw.size(), kw) != 0) return false;
  if (s.size() == kw.size()) return true;
  const char next = s[kw.size()];
  return !std::isalnum(static_cast<unsigned char>(next)) && next != '_';
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
  std::vector<ScopedSmartVar> vars;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;
  std::vector<std::size_t> loop_depths;

  auto update_depth_and_pop = [&](const std::string &l) {
    brace_depth += count_char(l, '{');
    const std::size_t closes = count_char(l, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    while (!loop_depths.empty() && brace_depth <= loop_depths.back()) {
      loop_depths.pop_back();
    }
  };

  for (const std::string &line : split_lines(source)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <memory>") {
      result.used_memory = true;
      continue;
    }

    const std::size_t before_depth = brace_depth;
    if (is_loop_kw(stripped, "for") || is_loop_kw(stripped, "while") ||
        is_loop_kw(stripped, "do")) {
      loop_depths.push_back(before_depth);
    }
    std::smatch match;
    static const std::regex heap_decl_re(
        R"(^(\s*)(?:std::)?(unique|shared)_ptr\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*\(\s*new\s+\3\s*\(([^)]*)\)\s*\)\s*;\s*$)");
    if (std::regex_match(line, match, heap_decl_re)) {
      result.used_memory = true;
      const std::string indent = match[1].str();
      const std::string kind = match[2].str();
      const std::string type = match[3].str();
      const std::string name = match[4].str();
      remember_var(vars, name, {type, kind == "shared"}, before_depth);
      for (const std::string &lowered : lower_heap_init(indent, kind, type, name, match[5].str())) {
        out.push_back(lowered);
      }
      update_depth_and_pop(line);
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
      remember_var(vars, name, {type, true}, before_depth);
      out.push_back(indent + "dpp_shared_ptr " + name + ";");
      out.push_back(indent + "dpp_shared_copy(&" + name + ", &" + source_name + ");");
      update_depth_and_pop(line);
      continue;
    }

    std::string lowered = lower_smart_exprs(line, live_vars(vars));
    const std::string lowered_stripped = trim(lowered);
    if (!vars.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      const std::vector<std::string> return_lines = append_cleanup_before_return(
          lowered, "dpp_memory_return_value", cleanup_lines(indent, vars));
      out.insert(out.end(), return_lines.begin(), return_lines.end());
      vars.clear();
    } else {
      const std::size_t opens = count_char(line, '{');
      const std::size_t closes = count_char(line, '}');
      if (!vars.empty() && closes > opens) {
        const std::size_t remaining_depth =
            closes > before_depth + opens ? std::size_t{0} : before_depth + opens - closes;
        const std::vector<std::string> cleanup =
            close_scope_lines(leading_indent(line), vars, remaining_depth);
        out.insert(out.end(), cleanup.begin(), cleanup.end());
      }
      if (!loop_depths.empty() && !vars.empty() &&
          (lowered_stripped == "break;" || lowered_stripped == "continue;")) {
        const std::vector<std::string> cleanup =
            peek_cleanup_lines(leading_indent(lowered), vars, loop_depths.back());
        out.insert(out.end(), cleanup.begin(), cleanup.end());
      }
      out.push_back(lowered);
    }

    update_depth_and_pop(line);
    if (before_depth == 1 && brace_depth == 0) {
      vars.clear();
    }
  }

  result.source = join_lines(out);
  if (!vars.empty()) {
    result.used_memory = true;
  }
  return result;
}

} // namespace dpp::convert
