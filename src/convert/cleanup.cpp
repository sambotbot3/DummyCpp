#include "dpp/convert/cleanup.h"

#include "dpp/string_utils.h"

#include <string>
#include <vector>

namespace dpp::convert {

std::vector<std::string> append_cleanup_before_return(const std::string &line,
                                                      const std::string &return_temp_name,
                                                      const std::vector<std::string> &cleanup_lines) {
  const std::string stripped = trim(line);
  if (stripped.rfind("return ", 0) != 0) {
    return {line};
  }

  const std::string indent = leading_indent(line);
  std::string expr = stripped.substr(std::string("return ").size());
  if (!expr.empty() && expr.back() == ';') {
    expr.pop_back();
  }

  std::vector<std::string> out;
  out.push_back(indent + "int " + return_temp_name + " = " + trim(expr) + ";");
  for (const std::string &cleanup : cleanup_lines) {
    out.push_back(cleanup);
  }
  out.push_back(indent + "return " + return_temp_name + ";");
  return out;
}

} // namespace dpp::convert
