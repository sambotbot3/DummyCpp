#include "dpp/convert/algorithm.h"

#include <map>
#include <regex>
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

std::string lower_algorithm_exprs(std::string line) {
  line = std::regex_replace(line, std::regex(R"(\bstd::min\s*\()"), "DPP_MIN(");
  line = std::regex_replace(line, std::regex(R"(\bstd::max\s*\()"), "DPP_MAX(");
  line = std::regex_replace(line, std::regex(R"(\bstd::swap\s*\()"), "DPP_SWAP(");
  return line;
}

} // namespace

AlgorithmResult lower_algorithms(const std::string &source) {
  AlgorithmResult result;
  std::map<std::string, std::string> vectors;
  std::vector<std::string> out;

  for (const std::string &line : split_lines(source)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <algorithm>") {
      result.used_algorithm = true;
      continue;
    }

    std::smatch match;
    static const std::regex vector_init_re(
        R"(^\s*dpp_vector_init\s*\(\s*&([A-Za-z_]\w*)\s*,\s*sizeof\s*\(\s*([A-Za-z_]\w*)\s*\)\s*\)\s*;\s*$)");
    if (std::regex_match(line, match, vector_init_re)) {
      vectors[match[1].str()] = match[2].str();
      out.push_back(line);
      continue;
    }

    static const std::regex sort_re(
        R"(^(\s*)(?:std::)?sort\s*\(\s*([A-Za-z_]\w*)\.begin\s*\(\s*\)\s*,\s*\2\.end\s*\(\s*\)\s*\)\s*;\s*$)");
    if (std::regex_match(line, match, sort_re) && vectors.count(match[2].str()) != 0) {
      result.used_algorithm = true;
      const std::string name = match[2].str();
      out.push_back(match[1].str() + "DPP_SORT_VECTOR(&" + name + ", " + vectors[name] + ");");
      continue;
    }

    static const std::regex reverse_re(
        R"(^(\s*)(?:std::)?reverse\s*\(\s*([A-Za-z_]\w*)\.begin\s*\(\s*\)\s*,\s*\2\.end\s*\(\s*\)\s*\)\s*;\s*$)");
    if (std::regex_match(line, match, reverse_re) && vectors.count(match[2].str()) != 0) {
      result.used_algorithm = true;
      const std::string name = match[2].str();
      out.push_back(match[1].str() + "DPP_REVERSE_VECTOR(&" + name + ", " + vectors[name] + ");");
      continue;
    }

    static const std::regex fill_re(
        R"(^(\s*)(?:std::)?fill\s*\(\s*([A-Za-z_]\w*)\.begin\s*\(\s*\)\s*,\s*\2\.end\s*\(\s*\)\s*,\s*(.+)\s*\)\s*;\s*$)");
    if (std::regex_match(line, match, fill_re) && vectors.count(match[2].str()) != 0) {
      result.used_algorithm = true;
      const std::string name = match[2].str();
      out.push_back(match[1].str() + "DPP_FILL_VECTOR(&" + name + ", " + vectors[name] + ", " +
                    trim(lower_algorithm_exprs(match[3].str())) + ");");
      continue;
    }

    std::string lowered = lower_algorithm_exprs(line);
    if (lowered != line) {
      result.used_algorithm = true;
    }
    out.push_back(lowered);
  }

  result.source = join_lines(out);
  return result;
}

} // namespace dpp::convert
