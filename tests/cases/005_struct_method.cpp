#include <iostream>

struct Point {
  int x;
  int y;
  int sum() const { return x + y; }
};

int main() {
  Point p{3, 6};
  std::cout << "sum=" << p.sum() << std::endl;
  return p.sum();
}
