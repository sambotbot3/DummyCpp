#include "dpp/convert/templates.h"
#include "dpp/string_utils.h"

#include <algorithm>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dpp::convert {
namespace {

struct Instantiation {
  const parser::FunctionTemplateDecl *decl = nullptr;
  std::vector<std::string> types;
};

std::string normalize_type(std::string type) {
  type = trim(type);
  type = std::regex_replace(type, std::regex(R"(\bstd::string\b)"), "string");
  return type;
}

std::string render_type(std::string type) {
  type = trim(type);
  if (type == "string") {
    return "std::string";
  }
  return type;
}

std::string c_name_type(std::string type) {
  type = normalize_type(type);
  type = std::regex_replace(type, std::regex(R"([^A-Za-z0-9_]+)"), "_");
  while (!type.empty() && type.front() == '_') {
    type.erase(type.begin());
  }
  while (!type.empty() && type.back() == '_') {
    type.pop_back();
  }
  return type.empty() ? "type" : type;
}

std::string instantiated_name(const parser::FunctionTemplateDecl &decl,
                              const std::vector<std::string> &types) {
  std::string name = decl.function.name;
  for (const std::string &type : types) {
    name += "__" + c_name_type(type);
  }
  return name;
}

std::map<std::string, std::string>
template_mapping(const parser::FunctionTemplateDecl &decl, const std::vector<std::string> &types) {
  std::map<std::string, std::string> mapping;
  for (std::size_t index = 0; index < decl.parameters.size(); ++index) {
    mapping[decl.parameters[index]] = normalize_type(types[index]);
  }
  return mapping;
}

std::string replace_template_type(std::string text,
                                  const std::map<std::string, std::string> &mapping) {
  for (const auto &entry : mapping) {
    text = std::regex_replace(text, std::regex("\\b" + entry.first + "\\b"),
                              render_type(entry.second));
  }
  return text;
}

bool has_unsupported_instantiated_parameter(const std::string &type) {
  return type.find("std::map") != std::string::npos ||
         type.find("std::unordered_map") != std::string::npos ||
         type.find("std::vector") != std::string::npos;
}

std::string lower_static_casts(std::string text) {
  static const std::regex cast_re(R"(static_cast\s*<\s*([^>]+)\s*>\s*\(\s*([^()]+)\s*\))");
  return std::regex_replace(text, cast_re, "(($1)($2))");
}

std::string render_instantiation(const Instantiation &instantiation) {
  const parser::FunctionTemplateDecl &decl = *instantiation.decl;
  const std::map<std::string, std::string> mapping = template_mapping(decl, instantiation.types);

  std::ostringstream out;
  out << replace_template_type(decl.function.return_type, mapping) << " "
      << instantiated_name(decl, instantiation.types) << "(";

  for (std::size_t index = 0; index < decl.function.parameters.size(); ++index) {
    const parser::FunctionParameter &parameter = decl.function.parameters[index];
    const std::string parameter_type = replace_template_type(parameter.type, mapping);
    if (has_unsupported_instantiated_parameter(parameter_type)) {
      throw std::runtime_error("template instantiation failed: unsupported parameter type '" +
                               parameter_type + "' in " + decl.function.name);
    }
    if (index != 0) {
      out << ", ";
    }
    out << parameter_type << " " << parameter.name;
  }

  out << ") {";
  out << lower_static_casts(replace_template_type(decl.function.body, mapping));
  out << "\n}\n";
  return out.str();
}

bool same_instantiation(const Instantiation &left, const Instantiation &right) {
  if (left.decl != right.decl || left.types.size() != right.types.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.types.size(); ++index) {
    if (normalize_type(left.types[index]) != normalize_type(right.types[index])) {
      return false;
    }
  }
  return true;
}

void add_instantiation(std::vector<Instantiation> &instantiations, Instantiation instantiation) {
  for (const Instantiation &existing : instantiations) {
    if (same_instantiation(existing, instantiation)) {
      return;
    }
  }
  instantiations.push_back(std::move(instantiation));
}

std::vector<std::string> split_template_args(const std::string &text) {
  std::vector<std::string> parts;
  for (const std::string &part : split_commas(text)) {
    parts.push_back(normalize_type(part));
  }
  return parts;
}

std::map<std::string, std::string> collect_simple_value_types(const std::string &source) {
  std::map<std::string, std::string> types;
  static const std::regex decl_re(
      R"(\b(?:const\s+)?(?:(std::string|string)|int|long|char|bool|double|float)\s+([A-Za-z_]\w*))");
  for (std::sregex_iterator it(source.begin(), source.end(), decl_re), end; it != end; ++it) {
    const std::string full = (*it)[0].str();
    const std::string name = (*it)[2].str();
    std::smatch type_match;
    if (std::regex_search(full, type_match,
                          std::regex(R"((std::string|string|int|long|char|bool|double|float))"))) {
      types[name] = normalize_type(type_match[1].str());
    }
  }
  return types;
}

std::string infer_simple_type(const std::string &expr, const std::map<std::string, std::string> &types) {
  const std::string value = trim(expr);
  if (std::regex_match(value, std::regex(R"(-?[0-9]+)"))) {
    return "int";
  }
  if (std::regex_match(value, std::regex(R"(-?[0-9]+L)"))) {
    return "long";
  }
  if (std::regex_match(value, std::regex(R"("([^"\\]|\\.)*")"))) {
    return "string";
  }
  const auto found = types.find(value);
  if (found != types.end()) {
    return found->second;
  }
  return "";
}

bool can_infer_single_parameter_call(const parser::FunctionTemplateDecl &decl) {
  if (decl.parameters.size() != 1 || decl.function.parameters.empty()) {
    return false;
  }
  const std::string template_parameter = decl.parameters[0];
  return trim(decl.function.parameters[0].type) == template_parameter ||
         trim(decl.function.parameters[0].type) == "const " + template_parameter + " &";
}

std::vector<Instantiation>
collect_instantiations(const parser::ParsedSource &parsed, const std::string &source_without_defs) {
  std::vector<Instantiation> instantiations;

  for (const parser::FunctionTemplateDecl &decl : parsed.function_templates) {
    const std::regex explicit_call("\\b" + decl.function.name + R"(\s*<\s*([^>]+)\s*>\s*\()");
    for (std::sregex_iterator it(source_without_defs.begin(), source_without_defs.end(), explicit_call),
         end;
         it != end; ++it) {
      std::vector<std::string> types = split_template_args((*it)[1].str());
      if (types.size() != decl.parameters.size()) {
        throw std::runtime_error("template instantiation failed: wrong argument count for " +
                                 decl.function.name);
      }
      add_instantiation(instantiations, {&decl, types});
    }
  }

  const std::map<std::string, std::string> value_types = collect_simple_value_types(source_without_defs);
  for (const parser::FunctionTemplateDecl &decl : parsed.function_templates) {
    if (!can_infer_single_parameter_call(decl)) {
      continue;
    }

    const std::regex inferred_call("\\b" + decl.function.name + R"(\s*\(\s*([^(),]+)\s*\))");
    for (std::sregex_iterator it(source_without_defs.begin(), source_without_defs.end(), inferred_call),
         end;
         it != end; ++it) {
      const std::string inferred = infer_simple_type((*it)[1].str(), value_types);
      if (!inferred.empty()) {
        add_instantiation(instantiations, {&decl, {inferred}});
      }
    }
  }

  return instantiations;
}

std::string remove_template_definitions(const parser::ParsedSource &parsed) {
  std::string out = parsed.text;
  for (auto it = parsed.function_templates.rbegin(); it != parsed.function_templates.rend(); ++it) {
    const std::size_t begin = it->range.begin.offset;
    const std::size_t end = it->range.end.offset;
    out.replace(begin, end - begin, "");
  }
  return out;
}

std::string replace_calls(std::string source, const std::vector<Instantiation> &instantiations) {
  for (const Instantiation &instantiation : instantiations) {
    const parser::FunctionTemplateDecl &decl = *instantiation.decl;
    const std::string concrete_name = instantiated_name(decl, instantiation.types);
    std::string explicit_pattern = "\\b" + decl.function.name + R"(\s*<\s*)";
    for (std::size_t index = 0; index < instantiation.types.size(); ++index) {
      if (index != 0) {
        explicit_pattern += R"(\s*,\s*)";
      }
      explicit_pattern += std::regex_replace(render_type(normalize_type(instantiation.types[index])),
                                             std::regex(R"(([.[\]{}()*+?^$|\\]))"), R"(\\$1)");
    }
    explicit_pattern += R"(\s*>\s*\()";
    source = std::regex_replace(source, std::regex(explicit_pattern), concrete_name + "(");

    if (decl.parameters.size() == 1) {
      source = std::regex_replace(source, std::regex("\\b" + decl.function.name + R"(\s*\()"),
                                  concrete_name + "(");
    }
  }
  return source;
}

std::string generated_instantiations(const std::vector<Instantiation> &instantiations) {
  std::ostringstream out;
  for (const Instantiation &instantiation : instantiations) {
    out << render_instantiation(instantiation) << "\n";
  }
  return out.str();
}

void reject_remaining_template_calls(const parser::ParsedSource &parsed, const std::string &source) {
  for (const parser::FunctionTemplateDecl &decl : parsed.function_templates) {
    const std::regex remaining("\\b" + decl.function.name + R"(\s*(?:<[^>]+>)?\s*\()");
    if (std::regex_search(source, remaining)) {
      throw std::runtime_error("template instantiation failed: could not resolve call to " +
                               decl.function.name);
    }
  }
}

} // namespace

TemplateResult lower_function_templates(const parser::ParsedSource &parsed) {
  TemplateResult result;
  if (parsed.function_templates.empty()) {
    result.source = parsed.text;
    return result;
  }

  std::string source = remove_template_definitions(parsed);
  const std::vector<Instantiation> instantiations = collect_instantiations(parsed, source);
  source = replace_calls(source, instantiations);
  reject_remaining_template_calls(parsed, source);

  const std::string generated = generated_instantiations(instantiations);
  if (!generated.empty()) {
    std::size_t insert_at = parsed.function_templates.front().range.begin.offset;
    for (const parser::FunctionTemplateDecl &decl : parsed.function_templates) {
      insert_at = std::min(insert_at, decl.range.begin.offset);
    }
    if (insert_at > source.size()) {
      insert_at = 0;
    }
    source.insert(insert_at, generated);
  }

  result.source = source;
  return result;
}

} // namespace dpp::convert
