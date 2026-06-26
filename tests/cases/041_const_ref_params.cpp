#include <iostream>

struct Point {
  int x;
  int y;
};

int sum_point(const Point &p) { return p.x + p.y; }

int scale_add(const int a, const int b) { return a + b; }

int main() {
  Point pt = {3, 4};
  std::cout << sum_point(pt) << std::endl;
  std::cout << scale_add(10, 20) << std::endl;
  return 0;
}
