#include "dpp/convert/records.h"
#include "dpp/string_utils.h"

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

// Count delimiters in a line, skipping string/char literal contents.
static void count_delimiters(const std::string &line, int &paren_depth, int &brace_depth) {
  bool in_str = false;
  char str_delim = 0;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (in_str) {
      if (c == '\\') { ++i; continue; }
      if (c == str_delim) in_str = false;
    } else if (c == '"' || c == '\'') {
      in_str = true;
      str_delim = c;
    } else if (c == '(') { ++paren_depth; }
    else if (c == ')') { --paren_depth; }
    else if (c == '{') { ++brace_depth; }
    else if (c == '}') { --brace_depth; }
  }
}

// Access specifier → emit standalone, don't accumulate
static bool is_access_specifier(const std::string &s) {
  return s == "public:" || s == "private:" || s == "protected:";
}

// Split a single line into multiple logical elements at top-level semicolons/braces.
// Used to normalise inline struct bodies (e.g., "int x, y; Ctor() {}").
static std::vector<std::string> split_line_at_top_level(const std::string &line) {
  std::vector<std::string> result;
  std::string cur;
  int pd = 0, bd = 0;
  bool in_str = false, in_char = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (in_str) {
      cur += c;
      if (c == '\\' && i + 1 < line.size()) cur += line[++i];
      else if (c == '"') in_str = false;
      continue;
    }
    if (in_char) {
      cur += c;
      if (c == '\\' && i + 1 < line.size()) cur += line[++i];
      else if (c == '\'') in_char = false;
      continue;
    }
    if (c == '"') { in_str = true; cur += c; continue; }
    if (c == '\'') { in_char = true; cur += c; continue; }
    if (c == '(') { ++pd; cur += c; continue; }
    if (c == ')') { if (pd > 0) --pd; cur += c; continue; }
    if (c == '{') { ++bd; cur += c; continue; }
    if (c == '}') {
      cur += c;
      if (bd > 0) --bd;
      if (pd == 0 && bd == 0) {
        const std::string t = trim(cur);
        if (!t.empty()) result.push_back(t);
        cur.clear();
      }
      continue;
    }
    if (c == ';' && pd == 0 && bd == 0) {
      cur += c;
      const std::string t = trim(cur);
      if (!t.empty()) result.push_back(t);
      cur.clear();
      continue;
    }
    cur += c;
  }
  const std::string t = trim(cur);
  if (!t.empty()) result.push_back(t);
  return result;
}

// Join multi-line methods/constructors so each element ends at a ';' or '}'
// with all parens and braces balanced. Access specifiers are emitted standalone.
static std::vector<std::string> flatten_record_body(const std::string &body) {
  std::vector<std::string> result;
  std::string current;
  int paren_depth = 0, brace_depth = 0;
  for (const std::string &raw : split_lines(body)) {
    const std::string line = trim(raw);
    if (line.empty()) continue;
    // Access specifiers break any in-progress accumulation (shouldn't normally happen)
    // and emit standalone.
    if (is_access_specifier(line)) {
      if (!current.empty()) { result.push_back(current); current.clear(); paren_depth = 0; brace_depth = 0; }
      result.push_back(line);
      continue;
    }
    // If this is the only line (no newlines in body) and it contains multiple
    // top-level semicolons/braces, split it before processing.
    const bool only_line = body.find('\n') == std::string::npos;
    if (only_line && current.empty() &&
        (line.find(';') != std::string::npos || line.find('{') != std::string::npos)) {
      for (const std::string &tok : split_line_at_top_level(line)) {
        result.push_back(tok);
      }
      continue;
    }
    current += (current.empty() ? "" : " ") + line;
    count_delimiters(line, paren_depth, brace_depth);
    if (paren_depth == 0 && brace_depth == 0) {
      const char last = current.back();
      if (last == ';' || last == '}') {
        result.push_back(current);
        current.clear();
      }
    }
  }
  if (!current.empty()) result.push_back(current);
  return result;
}

struct Method {
  std::string return_type;
  std::string name;
  std::string params;
  std::string body;
  bool is_const = false;
  bool is_static = false;
};

struct OperatorMethod {
  std::string return_type; // e.g., "bool"
  std::string c_suffix;    // e.g., "eq", "ne", "lt"
  std::string op_sym;      // e.g., "==", "!="
  std::string params;      // raw C++ params, e.g., "const Point &o"
  std::string body;
  bool is_const = false;
};

struct StaticField {
  std::string type;
  std::string name;
};

struct VectorParam {
  std::size_t index = 0;
  std::string elem_type;
  std::string name;
  bool is_const = false;
};

struct Constructor {
  std::string params;           // raw C++ params with defaults stripped (for C signature)
  std::string params_with_defaults; // original params including "= value" defaults
  std::vector<std::string> default_values; // per-param default expressions ("" if no default)
  std::vector<std::pair<std::string, std::string>> initializers;
  std::string body; // statements after the initializer list
};

struct MapField {
  std::string name;
  std::string key_type;
  std::string val_type;
  bool unordered = false;
};

struct VectorField {
  std::string name;
  std::string elem_type;
};

struct Record {
  std::string name;
  std::vector<std::string> base_names;
  bool was_class = false;
  std::vector<std::string> fields;
  std::vector<std::string> field_names;
  std::map<std::string, std::string> field_defaults; // field_name → default value expression
  std::vector<StaticField> static_fields;
  std::vector<MapField> map_fields;
  std::vector<VectorField> vector_fields;
  std::vector<Method> methods;
  std::vector<Constructor> constructors;
  std::vector<OperatorMethod> operators;
  std::string destructor_body; // user-supplied ~Type() body, "" if none
};

bool is_string_field_decl(const std::string &field);

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

// Split "TYPE NAME = DEFAULT;" into ("TYPE NAME;", "DEFAULT").
// Returns ("", "") if no default is present.
std::pair<std::string, std::string> split_field_default(const std::string &line) {
  int angle = 0, paren = 0;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '<') { ++angle; continue; }
    if (c == '>') { if (angle > 0) --angle; continue; }
    if (c == '(') { ++paren; continue; }
    if (c == ')') { if (paren > 0) --paren; continue; }
    if (c == '=' && angle == 0 && paren == 0) {
      if (i + 1 < line.size() && line[i + 1] == '=') { ++i; continue; } // ==
      if (i > 0) {
        const char prev = line[i - 1];
        if (prev == '!' || prev == '<' || prev == '>' || prev == '=') continue;
      }
      const std::string decl = trim(line.substr(0, i)) + ";";
      std::string dval = trim(line.substr(i + 1));
      if (!dval.empty() && dval.back() == ';') dval.pop_back();
      return {decl, trim(dval)};
    }
  }
  return {line, ""};
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

// Split an expression on top-level `+` operators (respects parens and string literals).
static std::vector<std::string> split_string_concat_parts(const std::string &expr) {
  std::vector<std::string> parts;
  std::string cur;
  int depth = 0;
  bool in_str = false;
  char str_delim = 0;
  for (std::size_t i = 0; i < expr.size(); ++i) {
    const char c = expr[i];
    if (in_str) {
      cur += c;
      if (c == '\\' && i + 1 < expr.size()) cur += expr[++i];
      else if (c == str_delim) in_str = false;
    } else if (c == '"' || c == '\'') {
      in_str = true; str_delim = c; cur += c;
    } else if (c == '(') { ++depth; cur += c; }
    else if (c == ')') { --depth; cur += c; }
    else if (c == '+' && depth == 0) {
      parts.push_back(trim(cur)); cur.clear();
    } else {
      cur += c;
    }
  }
  parts.push_back(trim(cur));
  return parts;
}

// Convert a string concat part to a C-string expression, given the set of string field paths.
// A path like "self->field" becomes "dpp_string_c_str(&self->field)".
// A const char * identifier or literal is returned as-is.
static std::string cstr_of_concat_part(const std::string &part,
                                       const std::vector<std::string> &str_paths) {
  const std::string p = trim(part);
  // String literal — fine as-is.
  if (!p.empty() && p.front() == '"') return p;
  // Known string field path → c_str.
  for (const std::string &path : str_paths) {
    if (p == path) return "dpp_string_c_str(&" + path + ")";
  }
  // Otherwise treat as const char * (param, literal, etc.).
  return p;
}

// Emit dpp_string assignment for a string field. If rhs is a concat (contains +),
// emits a temp-variable block; otherwise emits a single assign_cstr.
static std::string lower_string_field_assign(const std::string &path,
                                              const std::string &rhs,
                                              const std::vector<std::string> &str_paths) {
  const std::vector<std::string> parts = split_string_concat_parts(rhs);
  if (parts.size() == 1) {
    return "dpp_string_assign_cstr(&" + path + ", " + cstr_of_concat_part(rhs, str_paths) + ");";
  }
  // Multi-part concat: use a temp to avoid aliasing when path == one of the parts.
  const std::string tmp = "_dpp_field_tmp";
  std::string result = "{ dpp_string " + tmp + "; dpp_string_init_cstr(&" + tmp + ", " +
                       cstr_of_concat_part(parts[0], str_paths) + ");";
  for (std::size_t i = 1; i < parts.size(); ++i) {
    result += " dpp_string_append_cstr(&" + tmp + ", " +
              cstr_of_concat_part(parts[i], str_paths) + ");";
  }
  result += " dpp_string_assign_cstr(&" + path + ", dpp_string_c_str(&" + tmp + "));"
            " dpp_string_destroy(&" + tmp + "); }";
  return result;
}

// Extract type identifier from a field declaration like "Address addr;" → "Address".
static std::string field_type_from_decl(const std::string &decl) {
  std::string cleaned = trim(decl);
  if (!cleaned.empty() && cleaned.back() == ';') cleaned.pop_back();
  const std::size_t last_space = cleaned.find_last_of(" \t*&");
  if (last_space == std::string::npos) return "";
  // Type is everything before the last identifier.
  std::string type_part = trim(cleaned.substr(0, last_space));
  // Strip const and pointers.
  type_part = std::regex_replace(type_part, std::regex(R"(\bconst\b)"), "");
  type_part = std::regex_replace(type_part, std::regex(R"([*&])"), "");
  return trim(type_part);
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

std::vector<VectorParam> collect_vector_ref_params(const std::string &params) {
  std::vector<VectorParam> vector_params;
  const std::vector<std::string> parts = split_commas(params);
  static const std::regex vector_ref_re(
      R"(^(const\s+)?(?:std::)?vector\s*<\s*([A-Za-z_]\w*)\s*>\s*&\s*([A-Za-z_]\w*)$)");
  for (std::size_t i = 0; i < parts.size(); ++i) {
    std::smatch match;
    if (std::regex_match(parts[i], match, vector_ref_re)) {
      vector_params.push_back({i, trim(match[2].str()), trim(match[3].str()), match[1].matched});
    }
  }
  return vector_params;
}

std::string lower_vector_push_value(const std::string &expr, const std::string &c_type) {
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

std::string lower_vector_ref_params(const std::string &params) {
  std::vector<std::string> parts = split_commas(params);
  const std::vector<VectorParam> vector_params = collect_vector_ref_params(params);
  for (const VectorParam &param : vector_params) {
    parts[param.index] = std::string(param.is_const ? "const " : "") + "dpp_vector *" + param.name;
  }

  std::ostringstream out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << parts[i];
  }
  return out.str();
}

std::string lower_vector_ref_body(std::string body, const std::string &params) {
  for (const VectorParam &param : collect_vector_ref_params(params)) {
    const std::string c_type = param.elem_type;
    const std::string at_fn = param.is_const ? "dpp_vector_const_at" : "dpp_vector_at";
    if (!param.is_const) {
      body = std::regex_replace(
          body,
          std::regex("\\b" + param.name + R"(\.push_back\s*\(([^()]*)\))"),
          "dpp_vector_push_back(" + param.name + ", &" +
              lower_vector_push_value("$1", c_type) + ")");
    }
    body = std::regex_replace(body, std::regex("\\b" + param.name + R"(\.size\s*\(\s*\))"),
                              "dpp_vector_size(" + param.name + ")");
    body = std::regex_replace(
        body, std::regex("\\b" + param.name + R"(\s*\[\s*([^\]]+)\s*\])"),
        "(*(" + std::string(param.is_const ? "const " : "") + c_type + " *)" + at_fn + "(" +
            param.name + ", $1))");
  }
  return body;
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

const Method *find_method(const std::map<std::string, Record> &records,
                          const std::string &type, const std::string &method_name) {
  const Record *record = find_record(records, type);
  if (record == nullptr) {
    return nullptr;
  }
  for (const Method &method : record->methods) {
    if (method.name == method_name) {
      return &method;
    }
  }
  for (const std::string &base_name : record->base_names) {
    if (const Method *method = find_method(records, base_name, method_name)) {
      return method;
    }
  }
  return nullptr;
}

std::string base_field_name(const Record &record, const std::string &base_name) {
  if (record.base_names.size() == 1) {
    return "base";
  }
  return "base_" + base_name;
}

bool find_base_path(const std::map<std::string, Record> &records, const Record &record,
                    const std::string &target, std::string &path) {
  for (const std::string &base_name : record.base_names) {
    const std::string field = base_field_name(record, base_name);
    if (base_name == target) {
      path = field;
      return true;
    }

    const Record *base = find_record(records, base_name);
    if (base == nullptr) {
      continue;
    }
    std::string nested_path;
    if (find_base_path(records, *base, target, nested_path)) {
      path = field + "." + nested_path;
      return true;
    }
  }
  return false;
}

std::string find_method_owner(const std::map<std::string, Record> &records,
                              const std::string &type, const std::string &method_name) {
  const Record *record = find_record(records, type);
  if (record == nullptr) {
    return type;
  }
  if (has_method(*record, method_name)) {
    return record->name;
  }
  for (const std::string &base_name : record->base_names) {
    const Record *base = find_record(records, base_name);
    if (base == nullptr) {
      continue;
    }
    const std::string owner = find_method_owner(records, base_name, method_name);
    if (owner != base_name || has_method(*base, method_name)) {
      return owner;
    }
  }
  return type;
}

// Replace all whole-word occurrences of `word` outside string/char literals.
static std::string replace_word_outside_strings(const std::string &src,
                                                const std::string &word,
                                                const std::string &replacement) {
  std::string result;
  bool in_str = false;
  char str_delim = 0;
  std::size_t i = 0;
  const auto is_word_char = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  };
  while (i < src.size()) {
    if (in_str) {
      result += src[i];
      if (src[i] == '\\' && i + 1 < src.size()) result += src[++i];
      else if (src[i] == str_delim) in_str = false;
      ++i;
    } else if (src[i] == '"' || src[i] == '\'') {
      in_str = true;
      str_delim = src[i];
      result += src[i++];
    } else {
      // Exclude member access: don't replace "field" in "obj.field" or "ptr->field"
      const char prev = i > 0 ? src[i - 1] : '\0';
      const bool at_start = i == 0 || (!is_word_char(prev) && prev != '.' && prev != '>');
      if (at_start && src.compare(i, word.size(), word) == 0) {
        const std::size_t end = i + word.size();
        if (end == src.size() || !is_word_char(src[end])) {
          result += replacement;
          i = end;
          continue;
        }
      }
      result += src[i++];
    }
  }
  return result;
}

void lower_inherited_field_refs(std::string &body, const Record &record,
                                const std::map<std::string, Record> &records,
                                const std::string &prefix) {
  for (const std::string &base_name : record.base_names) {
    const Record *base = find_record(records, base_name);
    if (base == nullptr) continue;
    const std::string base_prefix = prefix + base_field_name(record, base_name) + ".";
    for (const std::string &field : base->field_names) {
      body = replace_word_outside_strings(body, field, "self->" + base_prefix + field);
    }
    lower_inherited_field_refs(body, *base, records, base_prefix);
  }
}

// Collect all "self->path" string field paths (including from base classes and
// embedded record-type fields).
static std::vector<std::string> collect_string_field_paths(
    const Record &record, const std::map<std::string, Record> &records,
    const std::string &prefix) {
  std::vector<std::string> paths;
  for (std::size_t fi = 0; fi < record.fields.size() && fi < record.field_names.size(); ++fi) {
    if (is_string_field_decl(record.fields[fi])) {
      paths.push_back(prefix + record.field_names[fi]);
    } else {
      // Embedded record field: recurse into it.
      const std::string ftype = field_type_from_decl(record.fields[fi]);
      const Record *sub = find_record(records, ftype);
      if (sub != nullptr) {
        const std::string sub_prefix = prefix + record.field_names[fi] + ".";
        auto sub_paths = collect_string_field_paths(*sub, records, sub_prefix);
        paths.insert(paths.end(), sub_paths.begin(), sub_paths.end());
      }
    }
  }
  for (const std::string &base_name : record.base_names) {
    const Record *base = find_record(records, base_name);
    if (base == nullptr) continue;
    const std::string sub_prefix = prefix + base_field_name(record, base_name) + ".";
    auto sub = collect_string_field_paths(*base, records, sub_prefix);
    paths.insert(paths.end(), sub.begin(), sub.end());
  }
  return paths;
}

std::string lower_field_refs(std::string body, const Record &record,
                             const std::map<std::string, Record> &records) {
  body = replace_all(body, "this->", "self->");
  for (const std::string &field : record.field_names) {
    body = replace_word_outside_strings(body, field, "self->" + field);
  }
  for (const MapField &mf : record.map_fields) {
    body = replace_word_outside_strings(body, mf.name, "self->" + mf.name);
  }
  for (const VectorField &vf : record.vector_fields) {
    body = replace_word_outside_strings(body, vf.name, "self->" + vf.name);
  }
  for (const StaticField &sf : record.static_fields) {
    body = replace_word_outside_strings(body, sf.name, record.name + "_" + sf.name);
  }
  lower_inherited_field_refs(body, record, records, "");
  body = replace_all(body, "self->self->", "self->");
  // Lower string method calls, cout chains, and assignments for each string field path.
  const std::vector<std::string> str_paths = collect_string_field_paths(record, records, "self->");
  for (const std::string &path : str_paths) {
    const std::string esc = std::regex_replace(path, std::regex(R"(\.)"), "\\.");
    // cout chain: << path → << dpp_string_c_str(&path)
    body = std::regex_replace(body, std::regex(R"(<<\s*)" + esc + R"(\b(?!\s*[\[.]))"),
                              "<< dpp_string_c_str(&" + path + ")");
    // Method calls on string fields.
    body = std::regex_replace(body, std::regex(esc + R"(\.c_str\s*\(\s*\))"),
                              "dpp_string_c_str(&" + path + ")");
    body = std::regex_replace(body, std::regex(esc + R"(\.size\s*\(\s*\))"),
                              "dpp_string_size(&" + path + ")");
    body = std::regex_replace(body, std::regex(esc + R"(\.empty\s*\(\s*\))"),
                              "(dpp_string_size(&" + path + ") == 0)");
    body = std::regex_replace(body, std::regex(esc + R"(\.length\s*\(\s*\))"),
                              "dpp_string_size(&" + path + ")");
    // Append: field += expr → dpp_string_append_cstr (process before = to avoid collision).
    {
      static const std::regex append_assign_re_tpl(R"(\bTOKEN\s*\+=\s*(.+?)\s*;)");
      const std::regex append_assign_re("\\b" + esc + R"(\s*\+=\s*(.+?)\s*;)");
      std::string new_body;
      std::string remaining = body;
      std::smatch am;
      while (std::regex_search(remaining, am, append_assign_re)) {
        new_body += am.prefix().str();
        const std::string rhs = am[1].str();
        new_body += "dpp_string_append_cstr(&" + path + ", " +
                    cstr_of_concat_part(rhs, str_paths) + ");";
        remaining = am.suffix().str();
      }
      body = new_body + remaining;
    }
    // Assignment to string field: `self->field = expr;` — lower concat if needed.
    {
      const std::regex assign_re("\\b" + esc + R"(\s*=\s*(.+?)\s*;)");
      std::string new_body;
      std::string remaining = body;
      std::smatch am;
      while (std::regex_search(remaining, am, assign_re)) {
        new_body += am.prefix().str();
        new_body += lower_string_field_assign(path, am[1].str(), str_paths);
        remaining = am.suffix().str();
      }
      body = new_body + remaining;
    }
  }
  return body;
}

static void parse_initializer_list(const std::string &init_list, Constructor &constructor) {
  const std::regex init_re(R"(([A-Za-z_]\w*)\s*\(([^)]*)\))");
  for (std::sregex_iterator it(init_list.begin(), init_list.end(), init_re), end; it != end; ++it) {
    constructor.initializers.push_back({trim((*it)[1].str()), trim((*it)[2].str())});
  }
}

// Parse param defaults from "int x = 0, bool b = true, int y" → stripped="int x, bool b, int y",
// defaults=["0","true",""].
static void parse_param_defaults(const std::string &params_raw, Constructor &ctor) {
  ctor.params_with_defaults = params_raw;
  if (params_raw.empty()) { ctor.params = ""; return; }
  std::vector<std::string> parts;
  std::string cur;
  int pd = 0;
  for (char c : params_raw) {
    if (c == '(') { ++pd; cur += c; }
    else if (c == ')') { --pd; cur += c; }
    else if (c == ',' && pd == 0) { parts.push_back(trim(cur)); cur.clear(); }
    else cur += c;
  }
  if (!trim(cur).empty()) parts.push_back(trim(cur));
  std::string stripped;
  for (const std::string &p : parts) {
    if (!stripped.empty()) stripped += ", ";
    const auto eq = p.find('=');
    if (eq != std::string::npos) {
      stripped += trim(p.substr(0, eq));
      ctor.default_values.push_back(trim(p.substr(eq + 1)));
    } else {
      stripped += p;
      ctor.default_values.push_back("");
    }
  }
  ctor.params = stripped;
}

bool parse_constructor_line(const std::string &line, const std::string &record_name,
                            Constructor &constructor) {
  std::smatch match;
  // Constructor with initializer list and any body.
  const std::regex ctor_init_re("^" + record_name + R"(\s*\(([^)]*)\)\s*:\s*(.*?)\s*\{(.*)\}\s*;?$)");
  if (std::regex_match(line, match, ctor_init_re)) {
    parse_param_defaults(trim(match[1].str()), constructor);
    parse_initializer_list(match[2].str(), constructor);
    constructor.body = trim(match[3].str());
    return true;
  }
  // Constructor with no initializer list but with body (possibly empty).
  const std::regex ctor_body_re("^" + record_name + R"(\s*\(([^)]*)\)\s*\{(.*)\}\s*;?$)");
  if (std::regex_match(line, match, ctor_body_re)) {
    parse_param_defaults(trim(match[1].str()), constructor);
    constructor.body = trim(match[2].str());
    return true;
  }
  return false;
}

static std::string op_c_suffix(const std::string &sym) {
  if (sym == "==") return "eq";
  if (sym == "!=") return "ne";
  if (sym == "<")  return "lt";
  if (sym == "<=") return "le";
  if (sym == ">")  return "gt";
  if (sym == ">=") return "ge";
  if (sym == "+")  return "add";
  if (sym == "-")  return "sub";
  if (sym == "*")  return "mul";
  if (sym == "/")  return "div";
  return "";
}

bool parse_operator_method_line(const std::string &line, OperatorMethod &op) {
  static const std::regex op_re(
      R"(^([A-Za-z_][\w\s:*&<>]*?)\s+operator\s*([=!<>+\-*\/]{1,2})\s*\(([^)]*)\)\s*(const)?\s*\{(.*)\}\s*;?$)");
  std::smatch m;
  if (!std::regex_match(line, m, op_re)) return false;
  const std::string sym = trim(m[2].str());
  const std::string suffix = op_c_suffix(sym);
  if (suffix.empty()) return false;
  op.return_type = trim(m[1].str());
  op.c_suffix    = suffix;
  op.op_sym      = sym;
  op.params      = trim(m[3].str());
  op.is_const    = m[4].matched;
  op.body        = trim(m[5].str());
  return true;
}

static bool parse_destructor_line(const std::string &line, const std::string &record_name,
                                   std::string &body_out) {
  const std::regex dtor_re("^~" + record_name + R"(\s*\(\s*\)\s*\{(.*)\}\s*;?$)");
  std::smatch m;
  if (!std::regex_match(line, m, dtor_re)) return false;
  body_out = trim(m[1].str());
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
  // Strip leading 'static' keyword; mark method as static.
  if (method.return_type.rfind("static", 0) == 0 &&
      (method.return_type.size() == 6 ||
       std::isspace(static_cast<unsigned char>(method.return_type[6])))) {
    method.is_static = true;
    method.return_type = trim(method.return_type.substr(6));
  }
  method.name = trim(match[2].str());
  method.params = trim(match[3].str());
  method.is_const = match[4].matched;
  method.body = trim(match[5].str());
  return true;
}

Record parse_record_body(const std::string &kind, const std::string &name,
                         const std::vector<std::string> &base_names, const std::string &body) {
  Record record;
  record.name = name;
  record.base_names = base_names;
  record.was_class = kind == "class";

  for (const std::string &line : flatten_record_body(body)) {
    if (line == "public:" || line == "private:" || line == "protected:") {
      continue;
    }

    Constructor constructor;
    if (parse_constructor_line(line, name, constructor)) {
      record.constructors.push_back(constructor);
      continue;
    }

    {
      std::string dtor_body;
      if (parse_destructor_line(line, name, dtor_body)) {
        record.destructor_body = dtor_body;
        continue;
      }
    }

    OperatorMethod op_method;
    if (parse_operator_method_line(line, op_method)) {
      record.operators.push_back(op_method);
      continue;
    }

    Method method;
    if (parse_method_line(line, method)) {
      record.methods.push_back(method);
      continue;
    }

    if (line.find('(') == std::string::npos && !line.empty() && line.back() == ';') {
      // Static field declarations: store name+type but don't add to the struct body.
      const std::string stripped_line = trim(line);
      if (stripped_line.rfind("static ", 0) == 0) {
        const std::string without_static = trim(stripped_line.substr(7));
        auto [decl, dval] = split_field_default(without_static);
        const std::string fname = field_name_from_decl(decl);
        std::string decl_no_semi = trim(decl);
        if (!decl_no_semi.empty() && decl_no_semi.back() == ';') decl_no_semi.pop_back();
        const auto sp = decl_no_semi.rfind(' ');
        const std::string ftype = sp != std::string::npos ? trim(decl_no_semi.substr(0, sp)) : "";
        record.static_fields.push_back({ftype, fname});
        (void)dval;
      } else {
        // Detect map fields: std::map<K,V> name; or std::unordered_map<K,V> name;
        static const std::regex map_field_re(
            R"(^(?:std::)?(unordered_map|map)\s*<\s*((?:std::)?[A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;\s*$)");
        // Detect vector fields: std::vector<T> name;
        static const std::regex vec_field_re(
            R"(^(?:std::)?vector\s*<\s*((?:std::)?[A-Za-z_]\w*)\s*>\s+([A-Za-z_]\w*)\s*;\s*$)");
        std::smatch mfm;
        if (std::regex_match(line, mfm, map_field_re)) {
          MapField mf;
          mf.unordered = (mfm[1].str() == "unordered_map");
          mf.key_type = mfm[2].str();
          mf.val_type = mfm[3].str();
          mf.name = mfm[4].str();
          record.map_fields.push_back(mf);
        } else if (std::regex_match(line, mfm, vec_field_re)) {
          VectorField vf;
          vf.elem_type = mfm[1].str();
          vf.name = mfm[2].str();
          record.vector_fields.push_back(vf);
        } else {
          // Handle multi-variable declarations like "int x, y;"
          // Split on commas (at top level) to get individual names after the type.
          auto [stripped_decl, default_val] = split_field_default(line);
          std::string decl_no_semi = trim(stripped_decl);
          if (!decl_no_semi.empty() && decl_no_semi.back() == ';') decl_no_semi.pop_back();
          // Check for comma outside angles/parens (multi-var decl)
          {
            int angle = 0, paren = 0;
            bool has_top_comma = false;
            for (char ch : decl_no_semi) {
              if (ch == '<') ++angle;
              else if (ch == '>') { if (angle > 0) --angle; }
              else if (ch == '(') ++paren;
              else if (ch == ')') { if (paren > 0) --paren; }
              else if (ch == ',' && angle == 0 && paren == 0) { has_top_comma = true; break; }
            }
            if (has_top_comma) {
              // Find the type prefix: everything up to and including the first whitespace-separated token(s)
              // Strategy: find first comma, extract type from everything before the first name
              const std::size_t first_space = decl_no_semi.find_first_of(" \t");
              const std::string type_prefix = first_space != std::string::npos
                  ? trim(decl_no_semi.substr(0, first_space)) : "";
              // Split the rest (after type) on commas
              const std::string names_str = first_space != std::string::npos
                  ? trim(decl_no_semi.substr(first_space + 1)) : decl_no_semi;
              // Parse comma-separated names
              std::istringstream iss(names_str);
              std::string tok;
              while (std::getline(iss, tok, ',')) {
                const std::string fname = trim(tok);
                if (!fname.empty()) {
                  record.fields.push_back(type_prefix + " " + fname + ";");
                  record.field_names.push_back(fname);
                }
              }
            } else {
              const std::string fname = field_name_from_decl(stripped_decl);
              record.fields.push_back(stripped_decl);
              record.field_names.push_back(fname);
              if (!default_val.empty()) {
                record.field_defaults[fname] = default_val;
              }
            }
          }
        }
      }
    }
  }

  return record;
}

std::string emit_record(const Record &record, const std::map<std::string, Record> &records) {
  std::ostringstream out;
  // Emit tentative definitions for static fields before the struct so that
  // the struct's methods can reference them without forward-declaration errors.
  for (const StaticField &sf : record.static_fields) {
    out << "static " << sf.type << " " << record.name << "_" << sf.name << ";\n";
  }
  out << "typedef struct " << record.name << " {\n";
  for (const std::string &base_name : record.base_names) {
    out << "  " << base_name << " " << base_field_name(record, base_name) << ";\n";
  }
  for (const std::string &field : record.fields) {
    out << "  " << field << "\n";
  }
  for (const MapField &mf : record.map_fields) {
    out << "  " << (mf.unordered ? "dpp_unordered_map" : "dpp_map") << " " << mf.name << ";\n";
  }
  for (const VectorField &vf : record.vector_fields) {
    out << "  dpp_vector " << vf.name << ";\n";
  }
  out << "} " << record.name << ";\n";

  // Build set of string field names for use in _init and _destroy.
  std::vector<std::string> string_field_names;
  for (std::size_t fi = 0; fi < record.fields.size(); ++fi) {
    if (is_string_field_decl(record.fields[fi]) && fi < record.field_names.size()) {
      string_field_names.push_back(record.field_names[fi]);
    }
  }

  // Helper: emit map/vector field inits (call inside _init functions).
  auto emit_container_field_inits = [&]() {
    for (const MapField &mf : record.map_fields) {
      const bool str_key = (mf.key_type == "std::string" || mf.key_type == "string");
      const std::string key_size = str_key ? "sizeof(const char *)"
                                           : "sizeof(" + mf.key_type + ")";
      const std::string val_size = "sizeof(" + mf.val_type + ")";
      if (mf.unordered) {
        const std::string hash = str_key ? "dpp_hash_cstr" : "dpp_hash_long";
        const std::string eq = str_key ? "dpp_equal_cstr" : "dpp_equal_long";
        out << "  dpp_unordered_map_init(&self->" << mf.name << ", " << key_size
            << ", " << val_size << ", " << hash << ", " << eq << ");\n";
      } else {
        const std::string cmp = str_key ? "dpp_compare_cstr" : "dpp_compare_long";
        out << "  dpp_map_init(&self->" << mf.name << ", " << key_size
            << ", " << val_size << ", " << cmp << ");\n";
      }
    }
    for (const VectorField &vf : record.vector_fields) {
      const bool is_str = (vf.elem_type == "std::string" || vf.elem_type == "string");
      const std::string elem_size = is_str ? "sizeof(dpp_string)" : "sizeof(" + vf.elem_type + ")";
      out << "  dpp_vector_init(&self->" << vf.name << ", " << elem_size << ");\n";
    }
  };
  // Keep old name for compatibility in the callsites below.
  auto &emit_map_field_inits = emit_container_field_inits;

  // When multiple constructors exist, suffix each _init with the param count to avoid
  // duplicate C function names (C has no overloading).
  const bool multi_ctor = record.constructors.size() > 1;
  auto ctor_init_name = [&](const Constructor &c) -> std::string {
    if (!multi_ctor) return record.name + "_init";
    const std::size_t n = c.params.empty() ? 0 : split_commas(c.params).size();
    return record.name + "_init_" + std::to_string(n);
  };

  for (const Constructor &constructor : record.constructors) {
    out << "\nstatic inline void " << ctor_init_name(constructor) << "(" << record.name << " *self";
    if (!constructor.params.empty()) {
      out << ", " << constructor.params;
    }
    out << ") {\n";
    emit_map_field_inits();
    // Default-init string fields not covered by the initializer list.
    std::vector<std::string> covered;
    for (const auto &initializer : constructor.initializers) {
      covered.push_back(initializer.first);
    }
    for (const std::string &sfname : string_field_names) {
      if (std::find(covered.begin(), covered.end(), sfname) == covered.end()) {
        out << "  dpp_string_init(&self->" << sfname << ");\n";
      }
    }
    // Build a map from field_name → declared type (for sub-record detection).
    std::map<std::string, std::string> field_type_map;
    for (std::size_t fi = 0; fi < record.fields.size() && fi < record.field_names.size(); ++fi) {
      field_type_map[record.field_names[fi]] = field_type_from_decl(record.fields[fi]);
    }
    for (const auto &initializer : constructor.initializers) {
      if (std::find(record.base_names.begin(), record.base_names.end(), initializer.first) !=
          record.base_names.end()) {
        out << "  " << initializer.first << "_init(&self->"
            << base_field_name(record, initializer.first) << ", " << initializer.second << ");\n";
      } else if (std::find(string_field_names.begin(), string_field_names.end(),
                           initializer.first) != string_field_names.end()) {
        out << "  dpp_string_init_cstr(&self->" << initializer.first << ", "
            << initializer.second << ");\n";
      } else {
        // Check if the field is of a known sub-record type.
        const auto fit = field_type_map.find(initializer.first);
        if (fit != field_type_map.end() && find_record(records, fit->second) != nullptr) {
          out << "  " << fit->second << "_init(&self->" << initializer.first << ", "
              << initializer.second << ");\n";
        } else {
          out << "  self->" << initializer.first << " = " << initializer.second << ";\n";
        }
      }
    }
    // Apply default member initializers for fields not covered by the initializer list.
    for (const std::string &fname : record.field_names) {
      if (std::find(covered.begin(), covered.end(), fname) != covered.end()) continue;
      const auto it = record.field_defaults.find(fname);
      if (it != record.field_defaults.end()) {
        out << "  self->" << fname << " = " << it->second << ";\n";
      }
    }
    // Emit the constructor body (e.g., Widget() { ++count; }).
    if (!constructor.body.empty()) {
      std::string body = lower_field_refs(constructor.body, record, records);
      out << "  " << body << "\n";
    }
    out << "}\n";
  }

  // If there are no explicit constructors but there are default member initializers
  // or map fields, emit a parameterless _init that applies them.
  if (record.constructors.empty() && (!record.field_defaults.empty() || !record.map_fields.empty() || !record.vector_fields.empty())) {
    out << "\nstatic inline void " << record.name << "_init(" << record.name << " *self) {\n";
    emit_map_field_inits();
    for (const std::string &sfname : string_field_names) {
      out << "  dpp_string_init(&self->" << sfname << ");\n";
    }
    for (const std::string &fname : record.field_names) {
      const auto it = record.field_defaults.find(fname);
      if (it != record.field_defaults.end()) {
        out << "  self->" << fname << " = " << it->second << ";\n";
      }
    }
    out << "}\n";
  }

  out << "\nstatic inline void " << record.name << "_destroy(void *value) {\n"
      << "  " << record.name << " *self = (" << record.name << " *)value;\n";
  for (const std::string &base_name : record.base_names) {
    out << "  " << base_name << "_destroy(&self->" << base_field_name(record, base_name) << ");\n";
  }
  for (const std::string &sfname : string_field_names) {
    out << "  dpp_string_destroy(&self->" << sfname << ");\n";
  }
  // Destroy embedded sub-record fields (those of a known record type).
  for (std::size_t fi = 0; fi < record.fields.size() && fi < record.field_names.size(); ++fi) {
    if (is_string_field_decl(record.fields[fi])) continue;
    const std::string ftype = field_type_from_decl(record.fields[fi]);
    if (!ftype.empty() && find_record(records, ftype) != nullptr) {
      out << "  " << ftype << "_destroy(&self->" << record.field_names[fi] << ");\n";
    }
  }
  for (const MapField &mf : record.map_fields) {
    out << "  " << (mf.unordered ? "dpp_unordered_map_destroy" : "dpp_map_destroy")
        << "(&self->" << mf.name << ");\n";
  }
  for (const VectorField &vf : record.vector_fields) {
    const bool is_str = (vf.elem_type == "std::string" || vf.elem_type == "string");
    if (is_str) {
      out << "  dpp_vector_destroy_strings(&self->" << vf.name << ");\n";
    } else {
      out << "  dpp_vector_destroy(&self->" << vf.name << ");\n";
    }
  }
  if (!record.destructor_body.empty()) {
    const std::string dtor_lowered = lower_field_refs(record.destructor_body, record, records);
    out << "  " << dtor_lowered << "\n";
  }
  out << "  (void)self;\n"
      << "}\n";

  for (const Method &method : record.methods) {
    if (method.is_static) {
      // Static methods: no self parameter; rewrite bare static field names
      out << "\nstatic inline " << method.return_type << " " << record.name << "_" << method.name
          << "(";
      if (!method.params.empty()) {
        out << lower_vector_ref_params(method.params);
      } else {
        out << "void";
      }
      std::string body = method.body;
      // In a static method, bare field names refer to static members.
      for (const StaticField &sf : record.static_fields) {
        body = std::regex_replace(body, std::regex("\\b" + sf.name + "\\b"),
                                  record.name + "_" + sf.name);
      }
      body = lower_vector_ref_body(body, method.params);
      out << ") {\n  " << body << "\n}\n";
    } else {
      out << "\nstatic inline " << method.return_type << " " << record.name << "_" << method.name
          << "(" << (method.is_const ? "const " : "") << record.name << " *self";
      if (!method.params.empty()) {
        out << ", " << lower_vector_ref_params(method.params);
      }
      std::string body = lower_field_refs(method.body, record, records);
      // Lower bare self-method calls: "method(args)" → "RecordName_method(self, args)"
      // Must NOT be preceded by . or -> (those are calls on other objects/fields).
      auto lower_bare_method_call = [&](const std::string &mname,
                                        const std::string &c_prefix,
                                        const std::string &self_arg) {
        std::string new_body2;
        std::size_t bi3 = 0;
        while (bi3 < body.size()) {
          const auto pos = body.find(mname, bi3);
          if (pos == std::string::npos) { new_body2 += body.substr(bi3); break; }
          const bool start_ok = (pos == 0) ||
              (!std::isalnum(static_cast<unsigned char>(body[pos-1])) && body[pos-1] != '_');
          const bool not_member = (pos == 0) ||
              (body[pos-1] != '.' && body[pos-1] != '>');
          std::size_t after = pos + mname.size();
          while (after < body.size() && body[after] == ' ') ++after;
          const bool has_paren = (after < body.size() && body[after] == '(');
          if (start_ok && not_member && has_paren) {
            new_body2 += body.substr(bi3, pos - bi3);
            new_body2 += c_prefix + mname + "(" + self_arg + ", ";
            bi3 = after + 1;
          } else {
            new_body2 += body.substr(bi3, pos - bi3 + 1);
            bi3 = pos + 1;
          }
        }
        body = new_body2;
        // Fix empty-arg trailing comma
        body = std::regex_replace(body,
            std::regex(c_prefix + mname + "\\(" + self_arg + R"(,\s*\))"),
            c_prefix + mname + "(" + self_arg + ")");
      };
      // Own methods
      for (const Method &m2 : record.methods) {
        lower_bare_method_call(m2.name, record.name + "_", "self");
      }
      // Inherited base-class methods
      for (const std::string &base_name : record.base_names) {
        const Record *base = find_record(records, base_name);
        if (base == nullptr) continue;
        const std::string base_self = "&self->" + base_field_name(record, base_name);
        for (const Method &m2 : base->methods) {
          lower_bare_method_call(m2.name, base_name + "_", base_self);
        }
      }
      // Lower dpp_string field method calls that lower_field_refs has made `self->field` form.
      for (const std::string &sfname : string_field_names) {
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + sfname + R"(\.c_str\s*\(\s*\))"),
            "dpp_string_c_str(&self->" + sfname + ")");
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + sfname + R"(\.size\s*\(\s*\))"),
            "dpp_string_size(&self->" + sfname + ")");
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + sfname + R"(\.empty\s*\(\s*\))"),
            "(dpp_string_size(&self->" + sfname + ") == 0)");
      }
      // Lower map field subscript accesses in method bodies.
      for (const MapField &mf : record.map_fields) {
        const bool str_key = (mf.key_type == "std::string" || mf.key_type == "string");
        const std::string kst = str_key ? "const char *" : mf.key_type;
        // const methods use find (read-only), non-const methods use get_or_insert
        const std::string lookup = method.is_const
            ? (mf.unordered ? "dpp_unordered_map_find" : "dpp_map_find")
            : (mf.unordered ? "dpp_unordered_map_get_or_insert" : "dpp_map_get_or_insert");
        // self->field[key] → (*(val_type *)lookup(&self->field, &(kst){key_expr}))
        // For string-key maps, key_expr may be a dpp_string — wrap in dpp_string_c_str.
        // Use a manual scan to find self->field[...] and replace with balanced bracket.
        const std::string field_prefix = "self->" + mf.name + "[";
        std::string new_body;
        std::size_t bi = 0;
        while (bi < body.size()) {
          const auto pos = body.find(field_prefix, bi);
          if (pos == std::string::npos) { new_body += body.substr(bi); break; }
          // Check word boundary before self
          if (pos > 0 && (std::isalnum(static_cast<unsigned char>(body[pos-1])) ||
                          body[pos-1] == '_' || body[pos-1] == '>')) {
            new_body += body[bi]; ++bi; continue;
          }
          new_body += body.substr(bi, pos - bi);
          // Find matching ]
          std::size_t bracket_open = pos + field_prefix.size() - 1;
          int depth = 0;
          std::size_t bend = bracket_open;
          for (; bend < body.size(); ++bend) {
            if (body[bend] == '[') ++depth;
            else if (body[bend] == ']') { if (--depth == 0) break; }
          }
          if (bend >= body.size()) { new_body += body[bi]; ++bi; continue; }
          const std::string key_raw = trim(body.substr(bracket_open + 1, bend - bracket_open - 1));
          std::string key_expr;
          if (str_key) {
            // If key looks like a dpp_string field access (param.field or param->field),
            // wrap in dpp_string_c_str.  Check both . and -> access forms.
            static const std::regex struct_field_re(R"([A-Za-z_]\w*(?:->|\.)[A-Za-z_]\w*)");
            if (std::regex_search(key_raw, struct_field_re)) {
              // Normalize . to -> since struct ref params become pointers
              std::string normalized = key_raw;
              normalized = std::regex_replace(normalized,
                  std::regex(R"(([A-Za-z_]\w*)\.([A-Za-z_]\w*))"), "$1->$2");
              key_expr = "dpp_string_c_str(&" + normalized + ")";
            } else {
              key_expr = key_raw;
            }
          } else {
            key_expr = key_raw;
          }
          const std::string cast_type = method.is_const
              ? "const " + mf.val_type + " *"
              : mf.val_type + " *";
          new_body += "(*(" + cast_type + ")" + lookup + "(&self->" + mf.name +
                      ", &(" + kst + "){" + key_expr + "}))";
          bi = bend + 1;
        }
        body = new_body;
        // Lower size/count on map fields
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + mf.name + R"(\.size\s*\(\s*\))"),
            (mf.unordered ? "dpp_unordered_map_size" : "dpp_map_size") +
                std::string("(&self->") + mf.name + ")");
      }
      // Lower vector field method calls in method bodies.
      for (const VectorField &vf : record.vector_fields) {
        const bool is_str = (vf.elem_type == "std::string" || vf.elem_type == "string");
        const std::string c_type = is_str ? "dpp_string" : vf.elem_type;
        const std::string fref = "self->" + vf.name;
        // size() and empty()
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + vf.name + R"(\.size\s*\(\s*\))"),
            "dpp_vector_size(&" + fref + ")");
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + vf.name + R"(\.empty\s*\(\s*\))"),
            "(dpp_vector_size(&" + fref + ") == 0)");
        // pop_back() and clear()
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + vf.name + R"(\.pop_back\s*\(\s*\)\s*;)"),
            "dpp_vector_pop_back(&" + fref + ");");
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + vf.name + R"(\.clear\s*\(\s*\)\s*;)"),
            "dpp_vector_clear(&" + fref + ");");
        // back() — read last element
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + vf.name + R"(\.back\s*\(\s*\))"),
            "(*(" + c_type + " *)dpp_vector_at(&" + fref +
                ", dpp_vector_size(&" + fref + ") - 1))");
        // push_back(expr) as statement: "self->vf.push_back(expr);"
        // Needs a temp variable since dpp_vector_push_back takes const void*.
        {
          const std::regex pb_re(R"((\s*)self->)" + vf.name +
                                 R"(\.push_back\s*\(([^;]*)\)\s*;)");
          std::smatch pbm;
          std::string new_body;
          std::string remaining = body;
          while (std::regex_search(remaining, pbm, pb_re)) {
            new_body += pbm.prefix().str();
            const std::string ind = pbm[1].str();
            const std::string arg = trim(pbm[2].str());
            const std::string tmp = "_dpp_vf_push_" + vf.name;
            if (is_str) {
              new_body += ind + "dpp_string_vector_push_cstr(&" + fref + ", " + arg + ");";
            } else {
              new_body += ind + "{ " + c_type + " " + tmp + " = " + arg + "; "
                          "dpp_vector_push_back(&" + fref + ", &" + tmp + "); }";
            }
            remaining = pbm.suffix().str();
          }
          body = new_body + remaining;
        }
        // at(i) and operator[](i) — subscript access
        const std::string at_cast = method.is_const
            ? "const " + c_type + " *" : c_type + " *";
        const std::string at_fn = method.is_const ? "dpp_vector_const_at" : "dpp_vector_at";
        body = std::regex_replace(
            body, std::regex(R"(\bself->)" + vf.name + R"(\.at\s*\()"),
            "(*(" + at_cast + ")" + at_fn + "(&" + fref + ", ");
        // Replace closing ) of at() — handled implicitly since we didn't close the paren yet.
        // Subscript: self->vf[i] → (*(c_type *)dpp_vector_at(&fref, i))
        {
          const std::string vpfx = fref + "[";
          std::string nb;
          std::size_t bi2 = 0;
          while (bi2 < body.size()) {
            const auto pos = body.find(vpfx, bi2);
            if (pos == std::string::npos) { nb += body.substr(bi2); break; }
            nb += body.substr(bi2, pos - bi2);
            std::size_t bopen = pos + vpfx.size() - 1;
            int depth = 0;
            std::size_t bend = bopen;
            for (; bend < body.size(); ++bend) {
              if (body[bend] == '[') ++depth;
              else if (body[bend] == ']') { if (--depth == 0) break; }
            }
            if (bend >= body.size()) { nb += body[bi2]; ++bi2; continue; }
            const std::string idx = body.substr(bopen + 1, bend - bopen - 1);
            if (is_str) {
              // String element: emit c_str() directly for cout compatibility
              nb += "dpp_string_c_str((" + at_cast + ")" + at_fn + "(&" + fref + ", " + idx + "))";
            } else {
              nb += "(*(" + at_cast + ")" + at_fn + "(&" + fref + ", " + idx + "))";
            }
            bi2 = bend + 1;
          }
          body = nb;
        }
      }
      body = lower_vector_ref_body(body, method.params);
      // Lower struct ref params: `const T &name` → `const T *name` in signature,
      // so `name.field` → `name->field` in body (lower_cpp_surface_types handles
      // the signature, but the body needs rewriting here while we know the param names).
      {
        static const std::regex struct_ref_re(
            R"(const\s+([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*))");
        for (std::sregex_iterator it(method.params.begin(), method.params.end(),
                                     struct_ref_re), end; it != end; ++it) {
          const std::string pname = (*it)[2].str();
          // Convert `pname.` → `pname->` (only when not already `->`)
          body = std::regex_replace(body, std::regex("\\b" + pname + R"(\.)"),
                                    pname + "->");
          // Assignment to a map entry via ptr: `= pname` → `= *pname`
          body = std::regex_replace(body, std::regex("=\\s*" + pname + "\\b(?!->|\\[|\\.)"),
                                    "= *" + pname);
        }
      }
      out << ") {\n"
          << "  " << body << "\n"
          << "}\n";
    }
  }

  // Emit operator overloads (operator==, operator!=, etc.)
  for (const OperatorMethod &op : record.operators) {
    std::string params = op.params;
    // Convert const T & param → const T * param so the signature is valid C
    params = std::regex_replace(params,
        std::regex(R"(const\s+([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*))"), "const $1 *$2");
    // Also convert non-const T & → T * just in case
    params = std::regex_replace(params,
        std::regex(R"(\b([A-Za-z_]\w*)\s*&\s*([A-Za-z_]\w*))"), "$1 *$2");
    out << "\nstatic inline " << op.return_type << " " << record.name << "_" << op.c_suffix
        << "(" << (op.is_const ? "const " : "") << record.name << " *self";
    if (!params.empty()) out << ", " << params;
    out << ") {\n";
    std::string body = lower_field_refs(op.body, record, records);
    // For params that are now pointers, convert param.field → param->field
    const std::string orig_params = op.params;
    static const std::regex ref_param_re(
        R"(\bconst\s+[A-Za-z_]\w*\s*&\s*([A-Za-z_]\w*)\b)");
    for (std::sregex_iterator it(orig_params.begin(), orig_params.end(), ref_param_re), end;
         it != end; ++it) {
      const std::string pname = (*it)[1].str();
      body = std::regex_replace(body, std::regex("\\b" + pname + R"(\.)"), pname + "->");
    }
    // *this == param → RecordName_eq(self, param) etc.
    for (const OperatorMethod &op2 : record.operators) {
      body = std::regex_replace(body,
          std::regex(R"(\*this\s*)" + std::string(op2.op_sym == "==" ? "==" : op2.op_sym == "!=" ? "!=" : "XX") + R"(\s*([A-Za-z_]\w*))"),
          record.name + "_" + op2.c_suffix + "(self, $1)");
    }
    // Generic *this dereference → (*self)
    body = std::regex_replace(body, std::regex(R"(\*this\b)"), "(*self)");
    out << "  " << body << "\n}\n";
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

std::vector<std::string> parse_base_names(const std::string &source, std::size_t name_end,
                                          std::size_t brace) {
  const std::string clause = trim(source.substr(name_end, brace - name_end));
  if (clause.empty() || clause[0] != ':') {
    return {};
  }
  std::vector<std::string> base_names;
  std::string rest = trim(clause.substr(1));
  std::size_t cursor = 0;
  while (cursor < rest.size()) {
    std::size_t comma = rest.find(',', cursor);
    std::string base_clause = trim(rest.substr(cursor, comma - cursor));
    base_clause = std::regex_replace(base_clause, std::regex(R"(\b(public|private|protected)\b)"), "");
    base_clause = trim(base_clause);
    if (!base_clause.empty()) {
      base_names.push_back(base_clause);
    }
    if (comma == std::string::npos) {
      break;
    }
    cursor = comma + 1;
  }
  return base_names;
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

    const std::vector<std::string> base_names = parse_base_names(source, name_end, brace);
    const Record record =
        parse_record_body(kind, name, base_names, source.substr(brace + 1, close - brace - 1));
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
    if (record == nullptr || record->base_names.empty()) {
      continue;
    }
    std::vector<const Record *> stack;
    for (const std::string &base_name : record->base_names) {
      const Record *base = find_record(records, base_name);
      if (base != nullptr) {
        stack.push_back(base);
      }
    }
    while (!stack.empty()) {
      const Record *base = stack.back();
      stack.pop_back();
      std::string path;
      if (!find_base_path(records, *record, base->name, path)) {
        continue;
      }
      for (const std::string &field : base->field_names) {
        source = std::regex_replace(source, std::regex("\\b" + var + "\\." + field + "\\b"),
                                    var + "." + path + "." + field);
      }
      for (const std::string &base_name : base->base_names) {
        const Record *next_base = find_record(records, base_name);
        if (next_base != nullptr) {
          stack.push_back(next_base);
        }
      }
    }
  }
  return source;
}

static std::size_t find_closing_paren(const std::string &s, std::size_t pos); // defined below

// Split "Type a(args1), b(args2);" into separate "Type a(args1);\nType b(args2);" lines.
std::string split_multivar_ctor_decls(const std::string &source,
                                      const std::map<std::string, Record> &records) {
  const std::vector<std::string> lines_in = split_lines(source);
  std::vector<std::string> out;
  for (const std::string &line : lines_in) {
    const std::string stripped = trim(line);
    const std::string indent = leading_indent(line);
    bool handled = false;
    for (const auto &entry : records) {
      const std::string &type = entry.first;
      // Quick prefix check before allocating regex
      if (stripped.rfind(type + " ", 0) != 0 && stripped.rfind(type + "\t", 0) != 0) continue;
      if (stripped.size() <= type.size() + 1) continue;
      // Find first '(' — if absent or followed immediately by nothing, skip
      const auto paren_pos = stripped.find('(', type.size());
      if (paren_pos == std::string::npos) continue;
      const auto close_pos = find_closing_paren(stripped, paren_pos);
      if (close_pos == std::string::npos) continue;
      // After the first arg list, look for a comma (multi-var) vs semicolon (single-var)
      std::size_t after = close_pos + 1;
      while (after < stripped.size() && stripped[after] == ' ') ++after;
      if (after >= stripped.size() || stripped[after] != ',') break; // not multi-var
      // Split all declarations
      std::size_t pos = type.size();
      bool ok = true;
      while (pos < stripped.size()) {
        while (pos < stripped.size() && std::isspace((unsigned char)stripped[pos])) ++pos;
        std::size_t name_start = pos;
        while (pos < stripped.size() &&
               (std::isalnum((unsigned char)stripped[pos]) || stripped[pos] == '_'))
          ++pos;
        if (pos == name_start) { ok = false; break; }
        const std::string varname = stripped.substr(name_start, pos - name_start);
        while (pos < stripped.size() && stripped[pos] == ' ') ++pos;
        if (pos >= stripped.size() || stripped[pos] != '(') { ok = false; break; }
        const std::size_t cp = find_closing_paren(stripped, pos);
        if (cp == std::string::npos) { ok = false; break; }
        const std::string args = stripped.substr(pos + 1, cp - pos - 1);
        out.push_back(indent + type + " " + varname + "(" + args + ");");
        pos = cp + 1;
        while (pos < stripped.size() && stripped[pos] == ' ') ++pos;
        if (pos < stripped.size() && (stripped[pos] == ',' || stripped[pos] == ';')) ++pos;
      }
      if (ok) { handled = true; break; }
    }
    if (!handled) out.push_back(line);
  }
  return join_lines(out);
}

std::string lower_constructor_locals(const std::string &source,
                                     const std::map<std::string, Record> &records) {
  std::string out = source;
  for (const auto &entry : records) {
    const std::string &type = entry.first;
    const Record &record = entry.second;
    const bool multi_ctor = record.constructors.size() > 1;
    const std::regex ctor_decl_re("(^|\\n)([ \\t]+)" + type +
                                  R"(\s+([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*;)");
    if (!multi_ctor) {
      out = std::regex_replace(out, ctor_decl_re,
                               "$1$2" + type + " $3;\n$2" + type + "_init(&$3, $4);");
    } else {
      // Count call-site args to select the right _init_N function.
      std::string rebuilt;
      std::size_t cursor = 0;
      for (std::sregex_iterator it(out.begin(), out.end(), ctor_decl_re), end;
           it != end; ++it) {
        const std::smatch &m = *it;
        rebuilt.append(out, cursor, static_cast<std::size_t>(m.position()) - cursor);
        const std::string prefix = m[1].str();
        const std::string indent = m[2].str();
        const std::string varname = m[3].str();
        const std::string args = m[4].str();
        const std::size_t n = args.empty() ? 0 : split_commas(args).size();
        const std::string init_fn = type + "_init_" + std::to_string(n);
        rebuilt += prefix + indent + type + " " + varname + ";\n";
        rebuilt += indent + init_fn + "(&" + varname;
        if (!args.empty()) rebuilt += ", " + args;
        rebuilt += ");";
        cursor = static_cast<std::size_t>(m.position() + m.length());
      }
      rebuilt.append(out, cursor, out.size() - cursor);
      out = rebuilt;
    }
  }
  return out;
}

bool is_string_field_decl(const std::string &field) {
  return std::regex_search(field, std::regex(R"(\b(?:std::)?string\s+[A-Za-z_]\w*\s*;)"));
}

std::string lower_aggregate_locals(const std::string &source,
                                   const std::map<std::string, Record> &records) {
  std::string out = source;
  for (const auto &entry : records) {
    const Record &record = entry.second;
    const std::regex aggregate_re("(^|\\n)([ \\t]+)" + record.name +
                                  R"(\s+([A-Za-z_]\w*)\s*\{([^;{}]*)\}\s*;)");
    std::string rebuilt;
    std::size_t cursor = 0;
    for (std::sregex_iterator it(out.begin(), out.end(), aggregate_re), end; it != end; ++it) {
      const std::smatch match = *it;
      rebuilt.append(out, cursor, static_cast<std::size_t>(match.position()) - cursor);
      const std::string prefix = match[1].str();
      const std::string indent = match[2].str();
      const std::string name = match[3].str();
      const std::vector<std::string> args = split_commas(match[4].str());
      rebuilt += prefix + indent + record.name + " " + name + ";";
      for (std::size_t index = 0; index < args.size() && index < record.field_names.size(); ++index) {
        const std::string field = record.field_names[index];
        if (index < record.fields.size() && is_string_field_decl(record.fields[index])) {
          rebuilt += "\n" + indent + "dpp_string_init_cstr(&" + name + "." + field + ", " +
                     trim(args[index]) + ");";
        } else {
          rebuilt += "\n" + indent + name + "." + field + " = " + trim(args[index]) + ";";
        }
      }
      cursor = static_cast<std::size_t>(match.position() + match.length());
    }
    rebuilt.append(out, cursor, std::string::npos);
    out = rebuilt;
  }
  return out;
}

// Scan forward from `pos` through balanced parens (pos points to opening '(').
// Returns the index of the matching ')' or std::string::npos on failure.
static std::size_t find_closing_paren(const std::string &s, std::size_t pos) {
  int depth = 0;
  bool in_str = false, in_char = false;
  for (std::size_t i = pos; i < s.size(); ++i) {
    const char c = s[i];
    if (in_str) {
      if (c == '\\') ++i;
      else if (c == '"') in_str = false;
      continue;
    }
    if (in_char) {
      if (c == '\\') ++i;
      else if (c == '\'') in_char = false;
      continue;
    }
    if (c == '"') { in_str = true; }
    else if (c == '\'') { in_char = true; }
    else if (c == '(') { ++depth; }
    else if (c == ')') { if (--depth == 0) return i; }
  }
  return std::string::npos;
}

std::string lower_method_calls(const std::string &source, const std::map<std::string, Record> &records) {
  std::string out = source;
  const std::map<std::string, std::string> vars = collect_local_record_vars(out, records);
  out = lower_base_field_access(out, records, vars);
  for (const auto &var_entry : vars) {
    const std::string &var = var_entry.first;
    const std::string &type = var_entry.second;
    const Record *record = find_record(records, type);
    if (record == nullptr) continue;

    // Manual scanner: find `var.method(` with balanced paren args.
    std::string rebuilt;
    rebuilt.reserve(out.size());
    std::size_t i = 0;
    while (i < out.size()) {
      // Word-boundary check before var
      if (i > 0 && (std::isalnum(static_cast<unsigned char>(out[i-1])) || out[i-1] == '_')) {
        rebuilt += out[i++]; continue;
      }
      // Match `var.`
      if (out.compare(i, var.size(), var) != 0 || i + var.size() >= out.size() ||
          out[i + var.size()] != '.') {
        rebuilt += out[i++]; continue;
      }
      std::size_t after_dot = i + var.size() + 1;
      // Match identifier (method name)
      std::size_t mend = after_dot;
      while (mend < out.size() && (std::isalnum(static_cast<unsigned char>(out[mend])) || out[mend] == '_'))
        ++mend;
      if (mend == after_dot) { rebuilt += out[i++]; continue; }
      // Skip optional whitespace then require '('
      std::size_t after_name = mend;
      while (after_name < out.size() && out[after_name] == ' ') ++after_name;
      if (after_name >= out.size() || out[after_name] != '(') { rebuilt += out[i++]; continue; }
      // Find matching ')'
      const std::size_t close = find_closing_paren(out, after_name);
      if (close == std::string::npos) { rebuilt += out[i++]; continue; }

      const std::string method_name = out.substr(after_dot, mend - after_dot);
      const std::string args_raw = out.substr(after_name + 1, close - after_name - 1);
      const std::string args = trim(args_raw);
      const std::string owner = find_method_owner(records, type, method_name);
      const Method *method = find_method(records, owner, method_name);
      std::vector<std::string> call_args = split_commas(args_raw);
      if (method != nullptr) {
        for (const VectorParam &param : collect_vector_ref_params(method->params)) {
          if (param.index < call_args.size() && call_args[param.index].rfind("&", 0) != 0) {
            call_args[param.index] = "&" + call_args[param.index];
          }
        }
      }
      std::string self_arg = "&" + var;
      if (owner != type) {
        std::string path;
        if (find_base_path(records, *record, owner, path)) {
          self_arg = "&" + var + "." + path;
        }
      }
      rebuilt += owner + "_" + method_name + "(" + self_arg;
      for (std::size_t k = 0; k < call_args.size(); ++k) {
        rebuilt += ", ";
        rebuilt += trim(call_args[k]);
      }
      rebuilt += ")";
      i = close + 1;
    }
    out = rebuilt;
  }

  // Post-pass: expand constructor call arguments to temp variables.
  // Pattern: "  Fn(&var, RecordType(args));" → "  RecordType _tmp; RecordType_init(&_tmp, args);\n  Fn(&var, &_tmp);"
  {
    const std::regex ctor_arg_re(
        R"(^(\s*)(\S.*?)\(\s*(&?[A-Za-z_]\w*)(\s*,\s*)([A-Za-z_]\w*)\s*\(([^()]*)\)\s*\)\s*;\s*$)");
    std::vector<std::string> lines = split_lines(out);
    std::vector<std::string> result;
    int tmp_counter = 0;
    for (const std::string &ln : lines) {
      std::smatch m;
      if (std::regex_match(ln, m, ctor_arg_re)) {
        const std::string ctor_type = m[5].str();
        if (records.count(ctor_type)) {
          const std::string ind = m[1].str();
          const std::string fn_call = m[2].str();
          const std::string first_arg = m[3].str();
          const std::string inner_args = m[6].str();
          const std::string tmp = "_dpp_arg_" + std::to_string(tmp_counter++);
          result.push_back(ind + ctor_type + " " + tmp + ";");
          result.push_back(ind + ctor_type + "_init(&" + tmp + ", " + inner_args + ");");
          result.push_back(ind + fn_call + "(" + first_arg + ", &" + tmp + ");");
          continue;
        }
      }
      result.push_back(ln);
    }
    out = join_lines(result);
  }

  // Lower operator usage sites: "a == b" → "RecordType_eq(&a, &b)" for known record vars.
  for (const auto &var_entry : vars) {
    const std::string &var = var_entry.first;
    const std::string &type = var_entry.second;
    const Record *record = find_record(records, type);
    if (record == nullptr || record->operators.empty()) continue;
    for (const OperatorMethod &op : record->operators) {
      // Match: var <op_sym> rhs — where rhs is an identifier (possibly &-prefixed)
      const std::string sym_esc = std::regex_replace(op.op_sym, std::regex(R"([=!<>+\-*\/])"), "\\$&");
      const std::regex op_re("\\b" + var + R"(\s*)" + sym_esc + R"(\s*((?:&\s*)?[A-Za-z_]\w*))");
      std::string out2;
      std::size_t last2 = 0;
      for (std::sregex_iterator it(out.begin(), out.end(), op_re), end; it != end; ++it) {
        out2 += out.substr(last2, static_cast<std::size_t>((*it).position()) - last2);
        std::string rhs = trim((*it)[1].str());
        // If rhs is another known record var, use &rhs; else use as-is (might already be pointer)
        const std::string rhs_arg = (vars.count(rhs) > 0) ? "&" + rhs : rhs;
        out2 += type + "_" + op.c_suffix + "(&" + var + ", " + rhs_arg + ")";
        last2 = static_cast<std::size_t>((*it).position()) + static_cast<std::size_t>((*it).length());
      }
      if (!out2.empty()) {
        out2 += out.substr(last2);
        out = out2;
      }
    }
  }

  return out;
}

} // namespace

// Rewrite ClassName::staticMember accesses and out-of-class static definitions.
std::string lower_static_member_accesses(const std::string &source,
                                         const std::map<std::string, Record> &records) {
  std::string out = source;
  for (const auto &entry : records) {
    const std::string &type = entry.first;
    const Record &record = entry.second;

    // Rewrite out-of-class static field definitions: "TYPE ClassName::field = value;"
    // These appear at file scope (no leading whitespace on the line).
    for (const StaticField &sf : record.static_fields) {
      // "int Widget::count = 0;" → "static int Widget::count = 0;"
      out = std::regex_replace(
          out,
          std::regex("(^|\\n)(" + sf.type + R"(\s+)" + type + "::" + sf.name +
                     R"((?:\s*=[^;]+)?\s*;))"),
          "$1static $2");
    }

    // Rewrite ClassName::member accesses (static fields and static methods).
    for (const StaticField &sf : record.static_fields) {
      out = std::regex_replace(out, std::regex("\\b" + type + "::" + sf.name + "\\b"),
                               type + "_" + sf.name);
    }
    for (const Method &method : record.methods) {
      if (method.is_static) {
        out = std::regex_replace(out, std::regex("\\b" + type + "::" + method.name + "\\b"),
                                 type + "_" + method.name);
      }
    }
  }
  return out;
}

// Emit _init calls for default-constructible records declared as "Type name;".
std::string lower_default_ctor_locals(const std::string &source,
                                      const std::map<std::string, Record> &records) {
  std::string out = source;
  for (const auto &entry : records) {
    const std::string &type = entry.first;
    const Record &record = entry.second;

    // Does this type have a parameterless _init (or all-defaulted constructor)?
    bool has_default_init = record.constructors.empty() &&
        (!record.field_defaults.empty() || !record.map_fields.empty() || !record.vector_fields.empty());
    std::string default_call_args; // extra args when constructor has all-default params
    for (const Constructor &ctor : record.constructors) {
      if (trim(ctor.params).empty()) { has_default_init = true; break; }
      if (!ctor.default_values.empty() &&
          std::all_of(ctor.default_values.begin(), ctor.default_values.end(),
                      [](const std::string &dv){ return !dv.empty(); })) {
        has_default_init = true;
        for (const std::string &dv : ctor.default_values) {
          if (!default_call_args.empty()) default_call_args += ", ";
          default_call_args += dv;
        }
        break;
      }
    }
    if (!has_default_init) continue;

    // When multiple constructors exist, the parameterless one is named _init_0.
    const bool multi_ctor = record.constructors.size() > 1;
    const std::string init_fn = multi_ctor ? type + "_init_0" : type + "_init";
    const std::string default_args_str = default_call_args;

    // "  Type name;" NOT already followed by any Type_init... call → inject default init.
    // Use line-by-line scan to avoid double-init when a parameterized init follows.
    std::vector<std::string> lines_out = split_lines(out);
    const std::regex decl_re("^([ \\t]+)" + type + R"(\s+([A-Za-z_]\w*)\s*;\s*$)");
    for (std::size_t li = 0; li < lines_out.size(); ++li) {
      std::smatch dm;
      if (!std::regex_match(lines_out[li], dm, decl_re)) continue;
      const std::string indent = dm[1].str();
      const std::string var = dm[2].str();
      // Check if the next non-empty line is any Type_init variant for this var.
      bool already_init = false;
      for (std::size_t nxt = li + 1; nxt < lines_out.size(); ++nxt) {
        const std::string nl = trim(lines_out[nxt]);
        if (nl.empty()) continue;
        if (nl.rfind(type + "_init", 0) == 0 && nl.find("&" + var) != std::string::npos) {
          already_init = true;
        }
        break;
      }
      if (already_init) continue;
      const std::string init_call = init_fn + "(&" + var +
          (default_args_str.empty() ? "" : ", " + default_args_str) + ");";
      lines_out.insert(lines_out.begin() + static_cast<std::ptrdiff_t>(li) + 1,
                       indent + init_call);
      ++li; // skip the injected line
    }
    out = join_lines(lines_out);
  }
  return out;
}

RecordsResult lower_records(const std::string &source) {
  RecordsResult result;
  std::map<std::string, Record> records;
  result.source = lower_record_declarations(source, records, result.lowered_records);
  result.source = lower_static_member_accesses(result.source, records);
  result.source = split_multivar_ctor_decls(result.source, records);
  result.source = lower_constructor_locals(result.source, records);
  result.source = lower_default_ctor_locals(result.source, records);
  result.source = lower_aggregate_locals(result.source, records);
  result.source = lower_method_calls(result.source, records);

  // Collect types that need explicit _destroy at scope exit.
  for (const auto &[name, rec] : records) {
    const bool has_string_fields =
        std::any_of(rec.fields.begin(), rec.fields.end(), is_string_field_decl);
    const bool needs_cleanup = !rec.map_fields.empty() || !rec.vector_fields.empty() ||
                               !rec.base_names.empty() || has_string_fields ||
                               !rec.destructor_body.empty();
    if (needs_cleanup) result.destructible_types.insert(name);
  }
  return result;
}

// ── Record lifetime (scope-exit cleanup) ───────────────────────────────────

namespace {

struct ScopedRecord {
  std::string type;
  std::string name;
  std::size_t depth;
};

static std::size_t count_closes(const std::string &line) {
  std::size_t n = 0;
  for (char c : line) { if (c == '}') ++n; }
  return n;
}

static std::size_t count_opens(const std::string &line) {
  std::size_t n = 0;
  for (char c : line) { if (c == '{') ++n; }
  return n;
}

} // anonymous namespace

std::string lower_record_lifetime(const std::string &source,
                                  const std::set<std::string> &destructible_types) {
  if (destructible_types.empty()) return source;

  // Build regex for matching declarations of destructible types.
  // Pattern: "  TypeName varname;" (indented, no parens/braces on line).
  // We use a combined type-set pattern.
  std::string type_alt;
  for (const std::string &t : destructible_types) {
    if (!type_alt.empty()) type_alt += "|";
    type_alt += t;
  }
  const std::regex decl_re(
      "^([ \\t]+)(" + type_alt + R"()\s+([A-Za-z_]\w*)\s*;\s*$)");

  std::vector<std::string> out;
  std::vector<ScopedRecord> scoped;
  std::size_t brace_depth = 0;
  bool inside_struct_def = false;
  std::vector<std::size_t> loop_depths; // depth of each enclosing loop's opening brace

  // Track whether the "next line" after a record decl is an _init call for it.
  std::string pending_type, pending_name;
  std::size_t pending_depth = 0;
  static const std::regex init_call_re(R"(^[ \t]+([A-Za-z_]\w*)_init[_0-9]*\s*\(&([A-Za-z_]\w*))");
  static const std::regex struct_start_re(
      R"(^(?:typedef\s+)?(?:struct|class)\b.*\{\s*$)");
  static const std::regex loop_kw_re(R"(^\s*(for|while|do)\b)");

  auto emit_destroy_for_depth_above = [&](const std::string &indent, std::size_t min_depth,
                                          bool remove_from_scoped) {
    std::vector<ScopedRecord> to_destroy;
    for (auto it = scoped.rbegin(); it != scoped.rend(); ++it) {
      if (it->depth > min_depth) to_destroy.push_back(*it);
    }
    for (const ScopedRecord &r : to_destroy) {
      out.push_back(indent + r.type + "_destroy(&" + r.name + ");");
    }
    if (remove_from_scoped) {
      scoped.erase(std::remove_if(scoped.begin(), scoped.end(),
                                  [min_depth](const ScopedRecord &r) {
                                    return r.depth > min_depth;
                                  }),
                   scoped.end());
    }
  };

  for (const std::string &line : split_lines(source)) {
    const std::size_t before_depth = brace_depth;
    const std::string stripped = trim(line);

    // Track whether we're inside a struct/class definition body.
    if (std::regex_search(stripped, struct_start_re)) inside_struct_def = true;

    // Track loop entry (for depth-limited cleanup on break/continue).
    if (std::regex_search(stripped, loop_kw_re)) loop_depths.push_back(before_depth);

    // If we were expecting an init call for `pending_name`, check this line.
    if (!pending_name.empty()) {
      std::smatch im;
      if (std::regex_search(line, im, init_call_re) &&
          im[1].str() == pending_type && im[2].str() == pending_name) {
        scoped.push_back({pending_type, pending_name, pending_depth});
      }
      pending_name.clear();
    }

    // Update brace depth.
    const std::size_t opens = count_opens(line);
    const std::size_t closes = count_closes(line);
    brace_depth += opens;
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;

    // Pop loop_depths for loops whose body has closed.
    while (!loop_depths.empty() && brace_depth <= loop_depths.back()) {
      loop_depths.pop_back();
    }

    // If depth decreased, destroy records that went out of scope (in LIFO order).
    if (closes > 0 && brace_depth < before_depth + opens) {
      const std::string indent = leading_indent(line);
      emit_destroy_for_depth_above(indent + "  ", brace_depth, /*remove=*/true);
      if (stripped == "}" && inside_struct_def) inside_struct_def = false;
      out.push_back(line);
      continue;
    }

    const std::string indent = leading_indent(line);

    // Before break/continue, destroy records in the innermost loop body only.
    // Handle both standalone `break;` / `continue;` AND inline `if (...) break;`.
    if (!scoped.empty() && !loop_depths.empty()) {
      const std::size_t loop_depth = loop_depths.back();
      if (stripped == "break;" || stripped == "continue;") {
        emit_destroy_for_depth_above(indent, loop_depth, /*remove=*/false);
      } else {
        // Inline form: "if (cond) break;" or "if (cond) continue;" → expand to block.
        static const std::regex inline_bc_re(
            R"(^(if\s*\(.*\))\s*(break|continue)\s*;\s*$)");
        std::smatch bcm;
        if (std::regex_match(stripped, bcm, inline_bc_re)) {
          const std::string kw = bcm[2].str();
          std::vector<ScopedRecord> to_destroy_bc;
          for (auto it = scoped.rbegin(); it != scoped.rend(); ++it) {
            if (it->depth > loop_depth) to_destroy_bc.push_back(*it);
          }
          out.push_back(indent + bcm[1].str() + " {");
          for (const ScopedRecord &r : to_destroy_bc) {
            out.push_back(indent + "  " + r.type + "_destroy(&" + r.name + ");");
          }
          out.push_back(indent + "  " + kw + ";");
          out.push_back(indent + "}");
          continue; // skip default push_back below
        }
      }
    }

    // Before return, destroy all in-scope records (LIFO) then clear.
    if (!scoped.empty() && stripped.rfind("return", 0) == 0 &&
        (stripped.size() == 6 || stripped[6] == ' ' || stripped[6] == ';')) {
      emit_destroy_for_depth_above(indent, /*min_depth=*/0, /*remove=*/false);
      scoped.clear();
    }

    // Detect record declarations (outside struct defs).
    if (!inside_struct_def) {
      std::smatch dm;
      if (std::regex_match(line, dm, decl_re)) {
        pending_type = dm[2].str();
        pending_name = dm[3].str();
        pending_depth = before_depth;
        out.push_back(line);
        continue;
      }
    }

    out.push_back(line);
  }

  return join_lines(out);
}

} // namespace dpp::convert
