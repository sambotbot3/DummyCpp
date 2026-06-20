#include "dpp/convert/vector.h"
#include "cleanup.h"
#include "dpp/string_utils.h"

#include <algorithm>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

struct ScopedVector {
  std::string name;
  std::string elem_type;
  std::size_t depth = 0;
};

struct RangeVector {
  std::string item_name;
  std::string vector_name;
  std::string elem_type;
  std::string index_name;
  std::size_t depth = 0;
};

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
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.empty\s*\(\s*\))"),
                              "(dpp_vector_size(&" + name + ") == 0)");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.front\s*\(\s*\))"),
                              "(*(" + c_type + " *)dpp_vector_at(&" + name + ", 0))");
    line = std::regex_replace(line, std::regex("\\b" + name + R"(\.back\s*\(\s*\))"),
                              "(*(" + c_type + " *)dpp_vector_at(&" + name +
                                  ", dpp_vector_size(&" + name + ") - 1))");
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

std::map<std::string, std::string> live_vectors(const std::vector<ScopedVector> &vectors) {
  std::map<std::string, std::string> live;
  for (const ScopedVector &vector : vectors) {
    live[vector.name] = vector.elem_type;
  }
  return live;
}

std::vector<std::string> destroy_lines(const std::string &indent,
                                       const std::vector<ScopedVector> &vectors) {
  std::vector<std::string> lines;
  for (auto it = vectors.rbegin(); it != vectors.rend(); ++it) {
    lines.push_back(indent + "dpp_vector_destroy(&" + it->name + ");");
  }
  return lines;
}

std::vector<std::string> close_scope_lines(const std::string &indent,
                                           std::vector<ScopedVector> &vectors,
                                           std::size_t remaining_depth) {
  std::vector<ScopedVector> closing;
  for (const ScopedVector &vector : vectors) {
    if (vector.depth > remaining_depth) {
      closing.push_back(vector);
    }
  }

  vectors.erase(std::remove_if(vectors.begin(), vectors.end(),
                               [remaining_depth](const ScopedVector &vector) {
                                 return vector.depth > remaining_depth;
                               }),
                vectors.end());
  return destroy_lines(indent, closing);
}

std::string lower_vector_range_exprs(std::string line, const std::vector<RangeVector> &ranges) {
  for (const RangeVector &range : ranges) {
    line = std::regex_replace(line, std::regex("\\b" + range.item_name + R"(\.)"),
                              range.item_name + "->");
  }
  return line;
}

} // namespace

VectorResult lower_vectors(const std::string &source) {
  VectorResult result;
  std::vector<ScopedVector> vectors;
  std::vector<RangeVector> range_vectors;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;

  auto scope_depth_after = [&](const std::string &line) {
    std::size_t after = brace_depth;
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    after += count_char(line, '{');
    return closes > after ? std::size_t{0} : after - closes;
  };

  for (const std::string &line : split_lines(source)) {
    const std::size_t before_depth = brace_depth;
    const std::string stripped = trim(line);
    if (stripped == "#include <vector>") {
      result.used_vector = true;
      scope_depth_after(line);
      continue;
    }

    std::smatch match;
    static const std::regex vector_copy_decl_re(
        R"(^(\s*)const\s+(?:std::)?vector\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*=\s*(.+)\s*;\s*$)");
    if (std::regex_match(line, match, vector_copy_decl_re)) {
      result.used_vector = true;
      vectors.push_back({match[3].str(), match[2].str(), before_depth});
      out.push_back(match[1].str() + "std::vector<" + match[2].str() + "> " + match[3].str() +
                    " = " + match[4].str() + ";");
      scope_depth_after(line);
      continue;
    }

    static const std::regex vector_decl_re(
        R"(^(\s*)(?:std::)?vector\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;\s*$)");
    if (std::regex_match(line, match, vector_decl_re)) {
      result.used_vector = true;
      const std::string indent = match[1].str();
      const std::string elem_type = match[2].str();
      const std::string name = match[3].str();
      vectors.push_back({name, elem_type, before_depth});
      out.push_back(indent + "dpp_vector " + name + ";");
      out.push_back(indent + "dpp_vector_init(&" + name + ", sizeof(" +
                    c_type_for_vector_elem(elem_type) + "));");
      scope_depth_after(line);
      continue;
    }

    static const std::regex vector_init_re(
        R"(^(\s*)(?:std::)?vector\s*<\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*(?:=)?\s*\{(.*)\}\s*;\s*$)");
    if (std::regex_match(line, match, vector_init_re)) {
      result.used_vector = true;
      const std::string indent = match[1].str();
      const std::string elem_type = match[2].str();
      const std::string c_type = c_type_for_vector_elem(elem_type);
      const std::string name = match[3].str();
      vectors.push_back({name, elem_type, before_depth});
      out.push_back(indent + "dpp_vector " + name + ";");
      out.push_back(indent + "dpp_vector_init(&" + name + ", sizeof(" + c_type + "));");
      for (const std::string &value : split_commas(match[4].str())) {
        out.push_back(indent + "dpp_vector_push_back(&" + name + ", &" +
                      lower_push_value(value, c_type) + ");");
      }
      scope_depth_after(line);
      continue;
    }

    static const std::regex range_for_re(
        R"(^(\s*)for\s*\(\s*const\s+([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)\s*\)\s*\{\s*$)");
    if (std::regex_match(line, match, range_for_re)) {
      const std::string vector_name = match[4].str();
      const auto found = std::find_if(vectors.begin(), vectors.end(),
                                      [&](const ScopedVector &vector) {
                                        return vector.name == vector_name;
                                      });
      if (found != vectors.end()) {
        result.used_vector = true;
        const std::string index = "dpp_vector_index_" + match[3].str();
        range_vectors.push_back(
            {match[3].str(), vector_name, found->elem_type, index, before_depth + 1});
        out.push_back(match[1].str() + "for (size_t " + index + " = 0; " + index +
                      " < dpp_vector_size(&" + vector_name + "); ++" + index + ") {");
        out.push_back(match[1].str() + "  const " + found->elem_type + " *" +
                      match[3].str() + " = (const " + found->elem_type +
                      " *)dpp_vector_const_at(&" + vector_name + ", " + index + ");");
        scope_depth_after(line);
        continue;
      }
    }

    std::string lowered = lower_vector_range_exprs(line, range_vectors);
    for (const ScopedVector &vector : vectors) {
      const std::string &name = vector.name;
      const std::string c_type = c_type_for_vector_elem(vector.elem_type);
      const std::regex clear_re("^(\\s*)" + name + R"(\.clear\s*\(\s*\)\s*;\s*$)");
      if (std::regex_match(lowered, match, clear_re)) {
        lowered = match[1].str() + "dpp_vector_clear(&" + name + ");";
      }
      const std::regex reserve_re("^(\\s*)" + name + R"(\.reserve\s*\((.*)\)\s*;\s*$)");
      if (std::regex_match(lowered, match, reserve_re)) {
        lowered = match[1].str() + "dpp_vector_reserve(&" + name + ", " +
                  trim(match[2].str()) + ");";
      }
      const std::regex resize_re("^(\\s*)" + name + R"(\.resize\s*\((.*)\)\s*;\s*$)");
      if (std::regex_match(lowered, match, resize_re)) {
        const std::vector<std::string> args = split_commas(match[2].str());
        if (args.size() == 2) {
          lowered = match[1].str() + "dpp_vector_resize_fill(&" + name + ", " +
                    trim(args[0]) + ", &" + lower_push_value(args[1], c_type) + ");";
        } else {
          lowered = match[1].str() + "dpp_vector_resize(&" + name + ", " +
                    trim(match[2].str()) + ");";
        }
      }
      const std::regex pop_re("^(\\s*)" + name + R"(\.pop_back\s*\(\s*\)\s*;\s*$)");
      if (std::regex_match(lowered, match, pop_re)) {
        lowered = match[1].str() + "dpp_vector_pop_back(&" + name + ");";
      }
      const std::regex push_re("^(\\s*)" + name + R"(\.push_back\s*\((.*)\)\s*;\s*$)");
      if (std::regex_match(lowered, match, push_re)) {
        lowered = match[1].str() + "dpp_vector_push_back(&" + name + ", &" +
                  lower_push_value(match[2].str(), c_type) + ");";
      }
    }

    lowered = lower_vector_exprs(lowered, live_vectors(vectors));
    const std::string lowered_stripped = trim(lowered);
    if (!vectors.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      std::string expr = lowered_stripped.substr(std::string("return ").size());
      if (!expr.empty() && expr.back() == ';') {
        expr.pop_back();
      }
      if (std::find_if(vectors.begin(), vectors.end(), [&](const ScopedVector &vector) {
            return vector.name == trim(expr);
          }) != vectors.end()) {
        out.push_back(lowered);
        scope_depth_after(line);
        continue;
      }
      const std::string indent = leading_indent(lowered);
      const std::vector<std::string> return_lines =
          append_cleanup_before_return(lowered, "dpp_return_value", destroy_lines(indent, vectors));
      out.insert(out.end(), return_lines.begin(), return_lines.end());
      scope_depth_after(line);
      continue;
    }

    const std::size_t opens = count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    if (!vectors.empty() && closes > opens) {
      const std::size_t remaining_depth =
          closes > before_depth + opens ? std::size_t{0} : before_depth + opens - closes;
      const std::vector<std::string> cleanup =
          close_scope_lines(leading_indent(line), vectors, remaining_depth);
      out.insert(out.end(), cleanup.begin(), cleanup.end());
      range_vectors.erase(std::remove_if(range_vectors.begin(), range_vectors.end(),
                                         [&](const RangeVector &range) {
                                           return range.depth > remaining_depth;
                                         }),
                          range_vectors.end());
    }
    out.push_back(lowered);
    scope_depth_after(line);
  }

  result.source = join_lines(out);
  if (!vectors.empty()) {
    result.used_vector = true;
  }
  return result;
}

} // namespace dpp::convert
