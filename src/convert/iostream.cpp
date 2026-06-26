#include "dpp/convert/iostream.h"
#include "dpp/string_utils.h"

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

bool is_string_literal(const std::string &value) {
  return value.size() >= 2 && value.front() == '"' && value.back() == '"';
}

bool is_size_expression(const std::string &value) {
  if (value.find("dpp_vector_at(") != std::string::npos ||
      value.find("dpp_vector_const_at(") != std::string::npos) {
    return false;
  }
  if (value.find("== 0") != std::string::npos || value.find("!= 0") != std::string::npos) {
    return false;
  }
  return value.find(".size()") != std::string::npos ||
         value.find("dpp_string_size(") != std::string::npos ||
         value.find("dpp_vector_size(") != std::string::npos ||
         value.find("dpp_map_size(") != std::string::npos ||
         value.find("dpp_unordered_map_size(") != std::string::npos;
}

bool is_c_string_expression(const std::string &value) {
  return value.find("dpp_string_c_str(") != std::string::npos ||
         value.find("dpp_map_key_at(") != std::string::npos || value.find("->name") != std::string::npos;
}

bool is_double_expression(const std::string &value) {
  if (value.find("double *") != std::string::npos) {
    return true;
  }
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '.') {
      continue;
    }
    const bool has_digit_before = i > 0 && std::isdigit(static_cast<unsigned char>(value[i - 1]));
    const bool has_digit_after =
        i + 1 < value.size() && std::isdigit(static_cast<unsigned char>(value[i + 1]));
    if (has_digit_before || has_digit_after) {
      return true;
    }
  }
  return false;
}

bool is_char_literal(const std::string &value) {
  return value.size() >= 3 && value.front() == '\'' && value.back() == '\'';
}

bool is_unsigned_expression(const std::string &value) {
  return value.find("unsigned") != std::string::npos ||
         starts_with(value, "(unsigned");
}

bool is_long_expression(const std::string &value) {
  if (value.size() < 2) return false;
  const char last = value.back();
  if ((last != 'L' && last != 'l') || !std::isdigit(static_cast<unsigned char>(value[value.size() - 2])))
    return false;
  return true;
}

// Split a cout chain on top-level << operators, respecting string/char literals
// and parenthesised sub-expressions so that "a << b" or (x << 1) are not split.
std::vector<std::string> split_cout_chain(const std::string &chain) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  bool in_string = false;
  bool in_char = false;
  int paren_depth = 0;

  for (std::size_t i = 0; i < chain.size(); ++i) {
    const char c = chain[i];
    if (in_string) {
      if (c == '\\') { ++i; }       // skip escape sequence
      else if (c == '"') { in_string = false; }
      continue;
    }
    if (in_char) {
      if (c == '\\') { ++i; }
      else if (c == '\'') { in_char = false; }
      continue;
    }
    if (c == '"') { in_string = true; continue; }
    if (c == '\'') { in_char = true; continue; }
    if (c == '(') { ++paren_depth; continue; }
    if (c == ')') { if (paren_depth > 0) --paren_depth; continue; }

    if (paren_depth == 0 && c == '<' && i + 1 < chain.size() && chain[i + 1] == '<') {
      parts.push_back(trim(chain.substr(start, i - start)));
      start = i + 2;
      ++i; // skip second '<'
    }
  }
  const std::string tail = trim(chain.substr(start));
  if (!tail.empty()) {
    parts.push_back(tail);
  }
  return parts;
}

std::string lower_cout_line(const std::string &line, bool &used_stdio) {
  const std::string stripped = trim(line);
  const std::string indent = leading_indent(line);
  std::string chain;

  if (starts_with(stripped, "std::cout")) {
    chain = trim(stripped.substr(std::string("std::cout").size()));
  } else if (starts_with(stripped, "cout")) {
    chain = trim(stripped.substr(std::string("cout").size()));
  } else {
    return line;
  }

  if (!starts_with(chain, "<<") || chain.back() != ';') {
    return line;
  }

  used_stdio = true;
  chain = trim(chain.substr(2, chain.size() - 3));
  std::string format;
  std::vector<std::string> args;
  const std::vector<std::string> parts = split_cout_chain(chain);
  for (const std::string &part : parts) {
    if (part.empty()) {
      continue;
    }
    if (part == "std::endl" || part == "endl") {
      format += "\\n";
    } else if (is_string_literal(part)) {
      format += "%s";
      args.push_back(part);
    } else if (is_c_string_expression(part)) {
      format += "%s";
      if (part.find("->name") != std::string::npos &&
          part.find("dpp_string_c_str(") == std::string::npos) {
        args.push_back("dpp_string_c_str(&" + part + ")");
      } else {
        args.push_back(part);
      }
    } else if (is_size_expression(part)) {
      format += "%zu";
      args.push_back(part);
    } else if (is_double_expression(part)) {
      format += "%g";
      args.push_back(part);
    } else if (is_char_literal(part)) {
      format += "%c";
      args.push_back(part);
    } else if (is_long_expression(part)) {
      format += "%ld";
      args.push_back(part);
    } else if (is_unsigned_expression(part)) {
      format += "%u";
      args.push_back(part);
    } else {
      format += "%d";
      args.push_back(part);
    }
  }

  std::ostringstream lowered;
  lowered << indent << "printf(\"" << format << "\"";
  for (const std::string &arg : args) {
    lowered << ", " << arg;
  }
  lowered << ");";
  return lowered.str();
}

} // namespace

IostreamResult lower_iostreams(const std::string &source) {
  IostreamResult result;
  std::istringstream in(source);
  std::ostringstream out;
  std::string line;

  while (std::getline(in, line)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <iostream>" || stripped == "using namespace std;") {
      result.used_stdio = true;
      continue;
    }
    out << lower_cout_line(line, result.used_stdio) << '\n';
  }

  result.source = out.str();
  return result;
}

} // namespace dpp::convert
