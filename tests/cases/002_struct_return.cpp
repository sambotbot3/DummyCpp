#include <iostream>

struct Point {
  int x;
  int y;
};

int add(int a, int b) {
  return a + b;
}

int main() {
  Point p{1, 2};
  std::cout << "sum=" << add(p.x, p.y) << std::endl;
  return add(p.x, p.y);
}

