#include "dpp/string_utils.h"

#include <sstream>

namespace dpp {

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

std::string leading_indent(const std::string &line) {
  const std::size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return line;
  }
  return line.substr(0, first);
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

std::vector<std::string> split_commas(const std::string &value) {
  std::vector<std::string> parts;
  std::size_t cursor = 0;
  int angle_depth = 0;
  int brace_depth = 0;
  int paren_depth = 0;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '<') {
      ++angle_depth;
    } else if (value[i] == '>' && angle_depth > 0) {
      --angle_depth;
    } else if (value[i] == '{') {
      ++brace_depth;
    } else if (value[i] == '}' && brace_depth > 0) {
      --brace_depth;
    } else if (value[i] == '(') {
      ++paren_depth;
    } else if (value[i] == ')' && paren_depth > 0) {
      --paren_depth;
    } else if (value[i] == ',' && angle_depth == 0 && brace_depth == 0 && paren_depth == 0) {
      parts.push_back(trim(value.substr(cursor, i - cursor)));
      cursor = i + 1;
    }
  }
  const std::string tail = trim(value.substr(cursor));
  if (!tail.empty()) {
    parts.push_back(tail);
  }
  return parts;
}

bool starts_with(const std::string &value, const std::string &prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

} // namespace dpp
