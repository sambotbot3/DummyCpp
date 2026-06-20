#pragma once

# include "header_value.h"

#include <vector>

class HeaderAdder : public HeaderValue {
public:
  HeaderAdder(int start) : HeaderValue(start) {}
  void add(std::vector<int> &values, int input) { values.push_back(input + value); }
};
