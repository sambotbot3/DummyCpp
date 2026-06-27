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
  bool skipping_lambda_sort = false;

  for (const std::string &line : split_lines(source)) {
    const std::string stripped = trim(line);
    if (skipping_lambda_sort) {
      if (stripped == "});") {
        skipping_lambda_sort = false;
      }
      continue;
    }
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

    // Inline lambda sort: whole lambda on one line ending with `});`
    static const std::regex lambda_sort_inline_re(
        R"(^(\s*)(?:std::)?sort\s*\(\s*([A-Za-z_]\w*)\.begin\s*\(\s*\)\s*,\s*\2\.end\s*\(\s*\)\s*,\s*\[.*\]\s*\(.*\)\s*\{.*\}\s*\)\s*;\s*$)");
    if (std::regex_match(line, match, lambda_sort_inline_re) && vectors.count(match[2].str()) != 0) {
      result.used_algorithm = true;
      const std::string name = match[2].str();
      out.push_back(match[1].str() + "DPP_SORT_VECTOR(&" + name + ", " + vectors[name] + ", " +
                    sort_comparator_for_type(vectors[name]) + ");");
      continue;
    }
    // Multi-line lambda sort: skip lines until `});`
    static const std::regex lambda_sort_re(
        R"(^\s*(?:std::)?sort\s*\(\s*([A-Za-z_]\w*)\.begin\s*\(\s*\)\s*,\s*\1\.end\s*\(\s*\)\s*,\s*\[.*\]\(.*$)");
    if (std::regex_match(line, match, lambda_sort_re) && vectors.count(match[1].str()) != 0) {
      result.used_algorithm = true;
      skipping_lambda_sort = true;
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

    // Expression-level find/count/accumulate lowering (work on any line)
    std::string lowered = lower_algorithm_exprs(line);
    for (const auto &entry : vectors) {
      const std::string &name = entry.first;
      const std::string &elem = entry.second;
      // find != end → bool true
      lowered = std::regex_replace(
          lowered,
          std::regex("(?:std::)?find\\s*\\(\\s*" + name +
                     R"(\.begin\s*\(\s*\)\s*,\s*)" + name +
                     R"(\.end\s*\(\s*\)\s*,\s*([^)]+)\)\s*!=\s*)" + name +
                     R"(\.end\s*\(\s*\))"),
          "dpp_find_" + elem + "(&" + name + ", $1)");
      // find == end → bool false
      lowered = std::regex_replace(
          lowered,
          std::regex("(?:std::)?find\\s*\\(\\s*" + name +
                     R"(\.begin\s*\(\s*\)\s*,\s*)" + name +
                     R"(\.end\s*\(\s*\)\s*,\s*([^)]+)\)\s*==\s*)" + name +
                     R"(\.end\s*\(\s*\))"),
          "(!dpp_find_" + elem + "(&" + name + ", $1))");
      // count
      lowered = std::regex_replace(
          lowered,
          std::regex("(?:std::)?count\\s*\\(\\s*" + name +
                     R"(\.begin\s*\(\s*\)\s*,\s*)" + name +
                     R"(\.end\s*\(\s*\)\s*,\s*([^)]+)\))"),
          "dpp_count_" + elem + "(&" + name + ", $1)");
      // accumulate
      lowered = std::regex_replace(
          lowered,
          std::regex("(?:std::)?accumulate\\s*\\(\\s*" + name +
                     R"(\.begin\s*\(\s*\)\s*,\s*)" + name +
                     R"(\.end\s*\(\s*\)\s*,\s*([^)]+)\))"),
          "dpp_accumulate_" + elem + "(&" + name + ", $1)");
      // *min_element / *max_element
      lowered = std::regex_replace(
          lowered,
          std::regex("\\*\\s*(?:std::)?min_element\\s*\\(\\s*" + name +
                     R"(\.begin\s*\(\s*\)\s*,\s*)" + name + R"(\.end\s*\(\s*\)\s*\))"),
          "dpp_min_element_" + elem + "(&" + name + ")");
      lowered = std::regex_replace(
          lowered,
          std::regex("\\*\\s*(?:std::)?max_element\\s*\\(\\s*" + name +
                     R"(\.begin\s*\(\s*\)\s*,\s*)" + name + R"(\.end\s*\(\s*\)\s*\))"),
          "dpp_max_element_" + elem + "(&" + name + ")");
    }
    if (lowered != line) {
      result.used_algorithm = true;
    }
    out.push_back(lowered);
  }

  result.source = join_lines(out);
  return result;
}

} // namespace dpp::convert
