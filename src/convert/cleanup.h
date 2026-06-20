#pragma once

#include <string>
#include <vector>

namespace dpp::convert {

std::vector<std::string> append_cleanup_before_return(const std::string &line,
                                                      const std::string &return_temp_name,
                                                      const std::vector<std::string> &cleanup_lines);

} // namespace dpp::convert
