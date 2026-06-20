#include "dpp/convert/algorithm.h"
#include "dpp/string_utils.h"

#include <map>
#include <regex>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

std::string lower_algorithm_exprs(std::string line) {
  line = std::regex_replace(line, std::regex(R"(\bstd::min\s*\()"), "DPP_MIN(");
  line = std::regex_replace(line, std::regex(R"(\bstd::max\s*\()"), "DPP_MAX(");
  line = std::regex_replace(line, std::regex(R"(\bstd::swap\s*\()"), "DPP_SWAP(");
  return line;
}

std::string sort_comparator_for_type(const std::string &type) {
  return "dpp_algorithm_compare_" + type;
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
      out.push_back(match[1].str() + "DPP_SORT_VECTOR(&" + name + ", " + vectors[name] + ", " +
                    sort_comparator_for_type(vectors[name]) + ");");
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
