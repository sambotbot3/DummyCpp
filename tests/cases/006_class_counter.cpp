#include <iostream>

class Counter {
public:
  int value;
  Counter(int start) : value(start) {}
  void inc() { value += 1; }
  int get() const { return value; }
};

int main() {
  Counter c(4);
  c.inc();
  std::cout << "counter=" << c.get() << std::endl;
  return c.get();
}
