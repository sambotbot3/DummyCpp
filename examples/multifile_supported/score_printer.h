#pragma once

#include "vector_role.h"

#include <iostream>
#include <vector>

class ScorePrinter : public VectorRole {
public:
  ScorePrinter(int start) : VectorRole(start) {}
  void print(const std::vector<int> &values) const { std::cout << "size=" << values.size() << " last=" << values[values.size() - 1] << " tag=" << amount << std::endl; }
};
