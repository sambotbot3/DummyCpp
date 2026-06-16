#include <iostream>
#include <vector>

struct Point {
  int x;
  int y;
};

int main() {
  Point first{5, 6};
  std::vector<Point> pts;
  pts.push_back(first);
  pts.push_back(Point{7, 8});
  std::cout << "y=" << pts[0].y << std::endl;
  return pts[0].x + pts[1].y;
}
