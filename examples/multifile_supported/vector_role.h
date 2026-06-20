#pragma once

class VectorRole {
public:
  int amount;
  VectorRole(int start) : amount(start) {}
  int tag() const { return amount; }
};
