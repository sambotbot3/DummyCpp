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
  std::unique_ptr<Box> box(new Box(5));
  box->add(3);
  std::cout << "unique=" << box->get() << std::endl;
  return box->get();
}
