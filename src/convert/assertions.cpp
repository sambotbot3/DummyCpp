#include "dpp/convert/assertions.h"

#include <regex>
#include <sstream>
#include <string>

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

} // namespace

AssertionsResult lower_assertions(const std::string &source) {
  AssertionsResult result;
  std::istringstream in(source);
  std::ostringstream out;
  std::string line;

  while (std::getline(in, line)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <cassert>" || stripped == "#include <assert.h>") {
      result.used_assert = true;
      continue;
    }
    if (std::regex_search(line, std::regex(R"(\bassert\s*\()"))) {
      result.used_assert = true;
    }
    out << line << '\n';
  }

  result.source = out.str();
  return result;
}

} // namespace dpp::convert
