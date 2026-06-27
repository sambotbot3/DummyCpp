#pragma once

#include <string>

namespace dpp::convert {

std::string lower_default_args(const std::string &source);
std::string lower_pairs(const std::string &source);
std::string lower_optionals(const std::string &source);
std::string lower_enums(const std::string &source);
std::string lower_auto_types(const std::string &source);
std::string lower_structs(const std::string &source);
std::string lower_main_signature(const std::string &source);
std::string lower_aggregate_initializers(const std::string &source);
std::string lower_cpp_surface_types(const std::string &source);

} // namespace dpp::convert
