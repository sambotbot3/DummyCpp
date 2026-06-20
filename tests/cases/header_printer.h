#pragma once

#include "header_value.h"

#include <iostream>
#include <vector>

class HeaderPrinter : public HeaderValue {
public:
  HeaderPrinter(int start) : HeaderValue(start) {}
  void print(const std::vector<int> &values) const { std::cout << "header_size=" << values.size() << " header_last=" << values[values.size() - 1] << " header_tag=" << value << std::endl; }
};
