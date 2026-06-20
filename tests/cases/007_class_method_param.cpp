#include <iostream>

class Accumulator {
public:
  int value;
  Accumulator(int start) : value(start) {}
  void add(int amount) { value += amount; }
  int get() const { return value; }
};

int main() {
  Accumulator acc(10);
  acc.add(7);
  std::cout << "value=" << acc.get() << std::endl;
  return acc.get();
}
