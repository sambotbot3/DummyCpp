#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dpp {

std::string trim(const std::string &value);
std::vector<std::string> split_lines(const std::string &source);
std::string join_lines(const std::vector<std::string> &lines);
std::string leading_indent(const std::string &line);
std::size_t count_char(const std::string &line, char ch);
std::vector<std::string> split_commas(const std::string &value);
bool starts_with(const std::string &value, const std::string &prefix);

} // namespace dpp
