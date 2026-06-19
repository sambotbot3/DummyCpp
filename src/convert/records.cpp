#include "dpp/convert/records.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace dpp::convert {
namespace {

struct Method {
  std::string return_type;
  std::string name;
  std::string params;
  std::string body;
  bool is_const = false;
};

struct Constructor {
  std::string params;
  std::vector<std::pair<std::string, std::string>> initializers;
};

struct Record {
  std::string name;
  std::string base_name;
  bool was_class = false;
  std::vector<std::string> fields;
  std::vector<std::string> field_names;
  std::vector<Method> methods;
  std::vector<Constructor> constructors;
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

bool starts_with_at(const std::string &source, std::size_t pos, const std::string &prefix) {
  return source.size() >= pos + prefix.size() && source.compare(pos, prefix.size(), prefix) == 0;
}

bool word_boundary_after(const std::string &source, std::size_t pos) {
  if (pos >= source.size()) {
    return true;
  }
  const unsigned char ch = static_cast<unsigned char>(source[pos]);
  return !std::isalnum(ch) && ch != '_';
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

std::string field_name_from_decl(const std::string &decl) {
  std::string cleaned = trim(decl);
  if (!cleaned.empty() && cleaned.back() == ';') {
    cleaned.pop_back();
  }
  const std::size_t last_space = cleaned.find_last_of(" \t*&");
  if (last_space == std::string::npos) {
    return cleaned;
  }
  return trim(cleaned.substr(last_space + 1));
}

std::string replace_all(std::string value, const std::string &from, const std::string &to) {
  if (from.empty()) {
    return value;
  }
  std::size_t pos = 0;
  while ((pos = value.find(from, pos)) != std::string::npos) {
    value.replace(pos, from.size(), to);
    pos += to.size();
  }
  return value;
}

bool has_method(const Record &record, const std::string &method_name) {
  for (const Method &method : record.methods) {
    if (method.name == method_name) {
      return true;
    }
  }
  return false;
}

const Record *find_record(const std::map<std::string, Record> &records, const std::string &name) {
  const auto found = records.find(name);
  if (found == records.end()) {
    return nullptr;
  }
  return &found->second;
}

std::string find_method_owner(const std::map<std::string, Record> &records,
                              const std::string &type, const std::string &method_name) {
  const Record *record = find_record(records, type);
  while (record != nullptr) {
    if (has_method(*record, method_name)) {
      return record->name;
    }
    if (record->base_name.empty()) {
      break;
    }
    record = find_record(records, record->base_name);
  }
  return type;
}

std::string lower_field_refs(std::string body, const Record &record,
                             const std::map<std::string, Record> &records) {
  body = replace_all(body, "this->", "self->");
  for (const std::string &field : record.field_names) {
    body = std::regex_replace(body, std::regex("\\b" + field + "\\b"), "self->" + field);
  }
  const Record *base = find_record(records, record.base_name);
  while (base != nullptr) {
    for (const std::string &field : base->field_names) {
      body = std::regex_replace(body, std::regex("\\b" + field + "\\b"),
                                "self->base." + field);
    }
    if (base->base_name.empty()) {
      break;
    }
    base = find_record(records, base->base_name);
  }
  body = replace_all(body, "self->self->", "self->");
  return body;
}

bool parse_constructor_line(const std::string &line, const std::string &record_name,
                            Constructor &constructor) {
  const std::regex ctor_re("^" + record_name + R"(\s*\(([^)]*)\)\s*:\s*(.*?)\s*\{\s*\}\s*;?$)");
  std::smatch match;
  if (!std::regex_match(line, match, ctor_re)) {
    return false;
  }
  constructor.params = trim(match[1].str());
  const std::string init_list = match[2].str();
  const std::regex init_re(R"(([A-Za-z_]\w*)\s*\(([^)]*)\))");
  for (std::sregex_iterator it(init_list.begin(), init_list.end(), init_re), end; it != end; ++it) {
    constructor.initializers.push_back({trim((*it)[1].str()), trim((*it)[2].str())});
  }
  if (constructor.initializers.empty()) {
    return false;
  }
  return true;
}

bool parse_method_line(const std::string &line, Method &method) {
  static const std::regex method_re(
      R"(^([A-Za-z_][\w\s:*&<>]*?)\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(const)?\s*\{(.*)\}\s*;?$)");
  std::smatch match;
  if (!std::regex_match(line, match, method_re)) {
    return false;
  }
  method.return_type = trim(match[1].str());
  method.name = trim(match[2].str());
  method.params = trim(match[3].str());
  method.is_const = match[4].matched;
  method.body = trim(match[5].str());
  return true;
}

Record parse_record_body(const std::string &kind, const std::string &name, const std::string &base_name,
                         const std::string &body) {
  Record record;
  record.name = name;
  record.base_name = base_name;
  record.was_class = kind == "class";

  for (const std::string &raw_line : split_lines(body)) {
    const std::string line = trim(raw_line);
    if (line.empty() || line == "public:" || line == "private:" || line == "protected:") {
      continue;
    }

    Constructor constructor;
    if (parse_constructor_line(line, name, constructor)) {
      record.constructors.push_back(constructor);
      continue;
    }

    Method method;
    if (parse_method_line(line, method)) {
      record.methods.push_back(method);
      continue;
    }

    if (line.find('(') == std::string::npos && !line.empty() && line.back() == ';') {
      record.fields.push_back(line);
      record.field_names.push_back(field_name_from_decl(line));
    }
  }

  return record;
}

std::string emit_record(const Record &record, const std::map<std::string, Record> &records) {
  std::ostringstream out;
  out << "typedef struct " << record.name << " {\n";
  if (!record.base_name.empty()) {
    out << "  " << record.base_name << " base;\n";
  }
  for (const std::string &field : record.fields) {
    out << "  " << field << "\n";
  }
  out << "} " << record.name << ";\n";

  for (const Constructor &constructor : record.constructors) {
    out << "\nstatic inline void " << record.name << "_init(" << record.name << " *self";
    if (!constructor.params.empty()) {
      out << ", " << constructor.params;
    }
    out << ") {\n";
    for (const auto &initializer : constructor.initializers) {
      if (initializer.first == record.base_name) {
        out << "  " << record.base_name << "_init(&self->base, " << initializer.second << ");\n";
      } else {
        out << "  self->" << initializer.first << " = " << initializer.second << ";\n";
      }
    }
    out << "}\n";
  }

  for (const Method &method : record.methods) {
    out << "\nstatic inline " << method.return_type << " " << record.name << "_" << method.name
        << "(" << (method.is_const ? "const " : "") << record.name << " *self";
    if (!method.params.empty()) {
      out << ", " << method.params;
    }
    out << ") {\n"
        << "  " << lower_field_refs(method.body, record, records) << "\n"
        << "}\n";
  }

  return out.str();
}

std::size_t find_matching_brace(const std::string &source, std::size_t open_brace) {
  int depth = 0;
  for (std::size_t i = open_brace; i < source.size(); ++i) {
    if (source[i] == '{') {
      ++depth;
    } else if (source[i] == '}') {
      --depth;
      if (depth == 0) {
        return i;
      }
    }
  }
  return std::string::npos;
}

std::string parse_base_name(const std::string &source, std::size_t name_end, std::size_t brace) {
  const std::string clause = trim(source.substr(name_end, brace - name_end));
  if (clause.empty() || clause[0] != ':') {
    return "";
  }
  static const std::regex base_re(R"(^:\s*(?:public\s+)?([A-Za-z_]\w*)\s*$)");
  std::smatch match;
  if (!std::regex_match(clause, match, base_re)) {
    return "";
  }
  return match[1].str();
}

std::string lower_record_declarations(const std::string &source,
                                      std::map<std::string, Record> &records,
                                      bool &changed) {
  std::ostringstream out;
  std::size_t cursor = 0;

  while (cursor < source.size()) {
    std::size_t pos = std::string::npos;
    std::string kind;
    for (std::size_t i = cursor; i < source.size(); ++i) {
      if ((starts_with_at(source, i, "struct") && word_boundary_after(source, i + 6)) ||
          (starts_with_at(source, i, "class") && word_boundary_after(source, i + 5))) {
        pos = i;
        kind = starts_with_at(source, i, "struct") ? "struct" : "class";
        break;
      }
    }
    if (pos == std::string::npos) {
      out << source.substr(cursor);
      break;
    }

    out << source.substr(cursor, pos - cursor);
    std::size_t name_start = pos + kind.size();
    while (name_start < source.size() && std::isspace(static_cast<unsigned char>(source[name_start]))) {
      ++name_start;
    }
    std::size_t name_end = name_start;
    while (name_end < source.size() &&
           (std::isalnum(static_cast<unsigned char>(source[name_end])) || source[name_end] == '_')) {
      ++name_end;
    }
    const std::string name = source.substr(name_start, name_end - name_start);
    std::size_t brace = source.find('{', name_end);
    if (name.empty() || brace == std::string::npos) {
      out << source.substr(pos, name_end - pos);
      cursor = name_end;
      continue;
    }
    const std::size_t close = find_matching_brace(source, brace);
    if (close == std::string::npos || close + 1 >= source.size() || source[close + 1] != ';') {
      out << source.substr(pos, close == std::string::npos ? source.size() - pos : close - pos + 1);
      cursor = close == std::string::npos ? source.size() : close + 1;
      continue;
    }

    const std::string base_name = parse_base_name(source, name_end, brace);
    const Record record =
        parse_record_body(kind, name, base_name, source.substr(brace + 1, close - brace - 1));
    records[record.name] = record;
    out << emit_record(record, records);
    changed = true;
    cursor = close + 2;
  }

  return out.str();
}

std::map<std::string, std::string> collect_local_record_vars(
    const std::string &source, const std::map<std::string, Record> &records) {
  std::map<std::string, std::string> vars;
  for (const auto &entry : records) {
    const std::string &type = entry.first;
    const std::regex decl_re("\\b" + type + R"(\s+([A-Za-z_]\w*)\b)");
    for (std::sregex_iterator it(source.begin(), source.end(), decl_re), end; it != end; ++it) {
      const std::string var = (*it)[1].str();
      if (var != type) {
        vars[var] = type;
      }
    }
  }
  return vars;
}

std::string lower_base_field_access(std::string source,
                                    const std::map<std::string, Record> &records,
                                    const std::map<std::string, std::string> &vars) {
  for (const auto &var_entry : vars) {
    const std::string &var = var_entry.first;
    const Record *record = find_record(records, var_entry.second);
    if (record == nullptr || record->base_name.empty()) {
      continue;
    }
    const Record *base = find_record(records, record->base_name);
    while (base != nullptr) {
      for (const std::string &field : base->field_names) {
        source = std::regex_replace(source, std::regex("\\b" + var + "\\." + field + "\\b"),
                                    var + ".base." + field);
      }
      if (base->base_name.empty()) {
        break;
      }
      base = find_record(records, base->base_name);
    }
  }
  return source;
}

std::string lower_constructor_locals(const std::string &source,
                                     const std::map<std::string, Record> &records) {
  std::string out = source;
  for (const auto &entry : records) {
    const std::string &type = entry.first;
    const std::regex ctor_decl_re("\\b" + type + R"(\s+([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*;)");
    out = std::regex_replace(out, ctor_decl_re, type + " $1;\n  " + type + "_init(&$1, $2);");
  }
  return out;
}

std::string lower_method_calls(const std::string &source, const std::map<std::string, Record> &records) {
  std::string out = source;
  const std::map<std::string, std::string> vars = collect_local_record_vars(out, records);
  out = lower_base_field_access(out, records, vars);
  for (const auto &var_entry : vars) {
    const std::string &var = var_entry.first;
    const std::string &type = var_entry.second;
    const Record *record = find_record(records, type);
    if (record == nullptr) {
      continue;
    }
    const std::regex call_re("\\b" + var + R"(\.([A-Za-z_]\w*)\s*\(([^()]*)\))");
    std::string rebuilt;
    std::size_t cursor = 0;
    for (std::sregex_iterator it(out.begin(), out.end(), call_re), end; it != end; ++it) {
      const std::smatch match = *it;
      rebuilt.append(out, cursor, static_cast<std::size_t>(match.position()) - cursor);
      const std::string method_name = match[1].str();
      const std::string args = trim(match[2].str());
      const std::string owner = find_method_owner(records, type, method_name);
      std::string self_arg = "&" + var;
      if (owner != type) {
        self_arg = "&" + var + ".base";
      }
      rebuilt += owner + "_" + method_name + "(" + self_arg;
      if (!args.empty()) {
        rebuilt += ", " + args;
      }
      rebuilt += ")";
      cursor = static_cast<std::size_t>(match.position() + match.length());
    }
    rebuilt.append(out, cursor, std::string::npos);
    out = rebuilt;
  }
  return out;
}

} // namespace

RecordsResult lower_records(const std::string &source) {
  RecordsResult result;
  std::map<std::string, Record> records;
  result.source = lower_record_declarations(source, records, result.lowered_records);
  result.source = lower_constructor_locals(result.source, records);
  result.source = lower_method_calls(result.source, records);
  return result;
}

} // namespace dpp::convert
