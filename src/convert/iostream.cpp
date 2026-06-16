#include "dpp/convert/iostream.h"

#include <sstream>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

std::string trim(const std::string &value) {
  const std::string whitespace = " \t\r\n";
  const std::size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos) {
    return "";
  }
  const std::size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

bool starts_with(const std::string &value, const std::string &prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool is_string_literal(const std::string &value) {
  return value.size() >= 2 && value.front() == '"' && value.back() == '"';
}

std::string leading_indent(const std::string &line) {
  const std::size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return line;
  }
  return line.substr(0, first);
}

std::vector<std::string> split_cout_chain(const std::string &chain) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < chain.size()) {
    const std::size_t pos = chain.find("<<", start);
    if (pos == std::string::npos) {
      parts.push_back(trim(chain.substr(start)));
      break;
    }
    parts.push_back(trim(chain.substr(start, pos - start)));
    start = pos + 2;
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

