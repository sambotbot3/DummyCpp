#include "dpp/parser/preprocessor.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace dpp::parser {
namespace {

struct IncludeDirective {
  bool is_include = false;
  bool is_angle = false;
  bool is_quoted = false;
  std::string path;
  std::string normalized;
};

struct CachedHeader {
  std::string source;
  std::vector<std::string> injected_headers;
  bool include_once = false;
};

struct Context {
  std::string root_dir;
  std::map<std::string, CachedHeader> cache;
  std::vector<std::string> active_stack;
  std::map<std::string, std::vector<std::string>> injected_headers;
  std::map<std::string, bool> injected_once;
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

bool starts_with(const std::string &value, const std::string &prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string read_file(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open header: " + path);
  }

  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

bool path_exists(const std::string &path) {
  struct stat info;
  return stat(path.c_str(), &info) == 0;
}

std::string canonical_path(const std::string &path) {
  char resolved[PATH_MAX];
  if (realpath(path.c_str(), resolved) == nullptr) {
    throw std::runtime_error("failed to resolve path '" + path + "': " + std::strerror(errno));
  }
  return resolved;
}

std::string dirname_of(const std::string &path) {
  const std::size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return ".";
  }
  if (slash == 0) {
    return "/";
  }
  return path.substr(0, slash);
}

std::string join_path(const std::string &dir, const std::string &path) {
  if (dir.empty() || dir == ".") {
    return path;
  }
  if (dir == "/") {
    return "/" + path;
  }
  return dir + "/" + path;
}

bool is_inside_root(const std::string &root, const std::string &path) {
  return path == root || starts_with(path, root + "/");
}

void require_no_nul(const std::string &path) {
  if (path.find('\0') != std::string::npos) {
    throw std::runtime_error("include path contains NUL byte");
  }
}

IncludeDirective parse_include_directive(const std::string &line) {
  IncludeDirective directive;
  std::size_t cursor = line.find_first_not_of(" \t");
  if (cursor == std::string::npos || line[cursor] != '#') {
    return directive;
  }

  ++cursor;
  while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
    ++cursor;
  }
  if (line.compare(cursor, 7, "include") != 0) {
    return directive;
  }
  cursor += 7;
  if (cursor < line.size() && (std::isalnum(static_cast<unsigned char>(line[cursor])) ||
                               line[cursor] == '_')) {
    return directive;
  }
  while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
    ++cursor;
  }
  if (cursor >= line.size()) {
    throw std::runtime_error("malformed include directive: " + trim(line));
  }

  directive.is_include = true;
  const char opener = line[cursor];
  const char closer = opener == '<' ? '>' : '"';
  if (opener != '<' && opener != '"') {
    throw std::runtime_error("malformed include directive: " + trim(line));
  }
  directive.is_angle = opener == '<';
  directive.is_quoted = opener == '"';
  const std::size_t end = line.find(closer, cursor + 1);
  if (end == std::string::npos) {
    throw std::runtime_error("malformed include directive: " + trim(line));
  }
  directive.path = line.substr(cursor + 1, end - cursor - 1);
  require_no_nul(directive.path);
  if (directive.path.empty()) {
    throw std::runtime_error("empty include path in directive: " + trim(line));
  }
  if (!trim(line.substr(end + 1)).empty()) {
    throw std::runtime_error("trailing tokens after include directive: " + trim(line));
  }
  directive.normalized = std::string("#include ") + opener + directive.path + closer;
  return directive;
}

bool is_pragma_once(const std::string &line) {
  std::size_t cursor = line.find_first_not_of(" \t");
  if (cursor == std::string::npos || line[cursor] != '#') {
    return false;
  }
  ++cursor;
  while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
    ++cursor;
  }
  if (line.compare(cursor, 6, "pragma") != 0) {
    return false;
  }
  cursor += 6;
  return trim(line.substr(cursor)) == "once";
}

bool parse_ifndef(const std::string &line, std::string &macro) {
  std::size_t cursor = line.find_first_not_of(" \t");
  if (cursor == std::string::npos || line[cursor] != '#') {
    return false;
  }
  ++cursor;
  while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
    ++cursor;
  }
  if (line.compare(cursor, 6, "ifndef") != 0) {
    return false;
  }
  cursor += 6;
  macro = trim(line.substr(cursor));
  return !macro.empty() && macro.find_first_of(" \t") == std::string::npos;
}

bool parse_define(const std::string &line, std::string &macro) {
  std::size_t cursor = line.find_first_not_of(" \t");
  if (cursor == std::string::npos || line[cursor] != '#') {
    return false;
  }
  ++cursor;
  while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
    ++cursor;
  }
  if (line.compare(cursor, 6, "define") != 0) {
    return false;
  }
  cursor += 6;
  macro = trim(line.substr(cursor));
  return !macro.empty() && macro.find_first_of(" \t") == std::string::npos;
}

bool is_endif(const std::string &line) {
  std::size_t cursor = line.find_first_not_of(" \t");
  if (cursor == std::string::npos || line[cursor] != '#') {
    return false;
  }
  ++cursor;
  while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
    ++cursor;
  }
  if (line.compare(cursor, 5, "endif") != 0) {
    return false;
  }
  cursor += 5;
  return trim(line.substr(cursor)).empty();
}

std::vector<std::string> significant_line_indexes(const std::vector<std::string> &lines) {
  std::vector<std::string> indexes;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string stripped = trim(lines[i]);
    if (!stripped.empty() && !starts_with(stripped, "//")) {
      indexes.push_back(std::to_string(i));
    }
  }
  return indexes;
}

std::string format_cycle(const std::vector<std::string> &stack, const std::string &path) {
  std::ostringstream out;
  bool in_cycle = false;
  for (const std::string &entry : stack) {
    if (entry == path) {
      in_cycle = true;
    }
    if (in_cycle) {
      out << entry << " -> ";
    }
  }
  out << path;
  return out.str();
}

std::string expand_source(Context &context, const std::string &source, const std::string &current_path,
                          bool is_header, bool &include_once,
                          std::vector<std::string> &direct_injected);

std::string expand_header(Context &context, const std::string &include_path,
                          const std::string &including_path) {
  if (!include_path.empty() && include_path[0] == '/') {
    throw std::runtime_error("absolute quoted include path is not supported: " + include_path);
  }

  const std::string candidate = join_path(dirname_of(including_path), include_path);
  if (!path_exists(candidate)) {
    throw std::runtime_error("missing header '" + include_path + "' included from " + including_path);
  }
  const std::string canonical = canonical_path(candidate);
  if (!is_inside_root(context.root_dir, canonical)) {
    throw std::runtime_error("quoted include escapes input root: " + canonical);
  }

  for (const std::string &active : context.active_stack) {
    if (active == canonical) {
      throw std::runtime_error("include cycle detected: " + format_cycle(context.active_stack, canonical));
    }
  }

  const auto cached = context.cache.find(canonical);
  if (cached != context.cache.end()) {
    if (cached->second.include_once && context.injected_once[canonical]) {
      return "";
    }
    context.injected_once[canonical] = true;
    return cached->second.source;
  }

  context.active_stack.push_back(canonical);
  bool include_once = false;
  std::vector<std::string> direct_injected;
  const std::string expanded =
      expand_source(context, read_file(canonical), canonical, true, include_once, direct_injected);
  context.active_stack.pop_back();

  context.cache[canonical] = {expanded, direct_injected, include_once};
  context.injected_headers[canonical] = direct_injected;
  context.injected_once[canonical] = true;
  return expanded;
}

std::string expand_source(Context &context, const std::string &source, const std::string &current_path,
                          bool is_header, bool &include_once,
                          std::vector<std::string> &direct_injected) {
  const std::vector<std::string> lines = split_lines(source);
  std::vector<bool> skip(lines.size(), false);

  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (is_header && is_pragma_once(lines[i])) {
      include_once = true;
      skip[i] = true;
    }
  }

  const std::vector<std::string> significant = significant_line_indexes(lines);
  if (is_header && significant.size() >= 3) {
    const std::size_t first = static_cast<std::size_t>(std::strtoul(significant[0].c_str(), nullptr, 10));
    const std::size_t second = static_cast<std::size_t>(std::strtoul(significant[1].c_str(), nullptr, 10));
    const std::size_t last =
        static_cast<std::size_t>(std::strtoul(significant[significant.size() - 1].c_str(), nullptr, 10));
    std::string ifndef_macro;
    std::string define_macro;
    if (parse_ifndef(lines[first], ifndef_macro) && parse_define(lines[second], define_macro) &&
        ifndef_macro == define_macro && is_endif(lines[last])) {
      include_once = true;
      skip[first] = true;
      skip[second] = true;
      skip[last] = true;
    }
  }

  std::vector<std::string> out;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (skip[i]) {
      continue;
    }

    const IncludeDirective include = parse_include_directive(lines[i]);
    if (!include.is_include) {
      out.push_back(lines[i]);
      continue;
    }

    if (include.is_angle) {
      out.push_back(include.normalized);
      continue;
    }

    const std::string candidate = join_path(dirname_of(current_path), include.path);
    if (!path_exists(candidate)) {
      throw std::runtime_error("missing header '" + include.path + "' included from " + current_path);
    }
    const std::string canonical = canonical_path(candidate);
    direct_injected.push_back(canonical);
    out.push_back("/* dpp include begin: " + include.path + " */");
    const std::vector<std::string> expanded_lines =
        split_lines(expand_header(context, include.path, current_path));
    for (const std::string &expanded_line : expanded_lines) {
      out.push_back(expanded_line);
    }
    out.push_back("/* dpp include end: " + include.path + " */");
  }
  return join_lines(out);
}

} // namespace

PreprocessResult preprocess_translation_unit_file(const std::string &input_path,
                                                  const std::string &source) {
  try {
    Context context;
    const std::string canonical_input = canonical_path(input_path);
    context.root_dir = dirname_of(canonical_input);
    bool include_once = false;
    std::vector<std::string> direct_injected;
    PreprocessResult result;
    result.source =
        expand_source(context, source, canonical_input, false, include_once, direct_injected);
    result.injected_headers = context.injected_headers;
    result.injected_headers[canonical_input] = direct_injected;
    return result;
  } catch (const std::exception &err) {
    throw std::runtime_error(std::string("preprocess failed\n  error: ") + err.what());
  }
}

} // namespace dpp::parser
