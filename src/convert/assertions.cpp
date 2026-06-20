#include "dpp/convert/assertions.h"
#include "dpp/string_utils.h"

#include <regex>
#include <sstream>
#include <string>

namespace dpp::convert {

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
