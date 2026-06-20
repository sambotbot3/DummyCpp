#pragma once

#include <map>
#include <string>
#include <vector>

namespace dpp::parser {

struct PreprocessResult {
  std::string source;
  std::map<std::string, std::vector<std::string>> injected_headers;
};

PreprocessResult preprocess_translation_unit_file(const std::string &input_path,
                                                  const std::string &source);

} // namespace dpp::parser
