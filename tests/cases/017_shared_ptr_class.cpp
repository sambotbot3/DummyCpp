#include <iostream>
#include <memory>

class Box {
public:
  int value;
  Box(int start) : value(start) {}
  void add(int amount) { value += amount; }
  int get() const { return value; }
};

int main() {
  std::shared_ptr<Box> first(new Box(2));
  std::shared_ptr<Box> second = first;
  second->add(4);
  std::cout << "shared=" << first->get() << std::endl;
  return first->get();
}
