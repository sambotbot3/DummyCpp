#include <iostream>
#include <vector>

struct Point {
  int x;
  int y;
};

int main() {
  std::vector<Point> pts;
  pts.push_back(Point{1, 2});
  pts.push_back(Point{3, 4});
  std::cout << "x=" << pts[1].x << std::endl;
  return pts[0].x + pts[1].y;
}
