#pragma once

#include "vector_role.h"

#include <vector>

class ScoreAdder : public VectorRole {
public:
  ScoreAdder(int start) : VectorRole(start) {}
  void add(std::vector<int> &values, int value) { values.push_back(value + amount); }
};
