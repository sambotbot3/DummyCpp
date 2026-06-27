#pragma once

#include <set>
#include <string>

namespace dpp::convert {

struct RecordsResult {
  std::string source;
  bool lowered_records = false;
  // Types that have non-trivial _destroy (string/map/vector fields or custom destructor).
  // lower_record_lifetime uses this to inject _destroy calls at scope exit.
  std::set<std::string> destructible_types;
};

RecordsResult lower_records(const std::string &source);
std::string lower_record_lifetime(const std::string &source,
                                  const std::set<std::string> &destructible_types);

} // namespace dpp::convert
