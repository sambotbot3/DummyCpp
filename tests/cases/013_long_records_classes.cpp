#include <iostream>

struct Point {
  int x;
  int y;
  int sum() const { return x + y; }
};

Point make_point(int base) {
  Point p{base, base + 1};
  return p;
}

int combine(Point a, Point b) {
  return a.sum() + b.sum();
}

class PairScore {
public:
  int left;
  int right;
  PairScore(int a, int b) : left(a), right(b) {}
  void add_left(int value) { left += value; }
  void add_right(int value) { right += value; }
  int total() const { return left + right; }
};

PairScore make_score(int base) {
  PairScore score(base, base + 2);
  score.add_left(1);
  return score;
}

int finish_score(PairScore score) {
  score.add_right(3);
  return score.total();
}

int main() {
  Point left = make_point(3);
  Point right = make_point(6);
  int point_total = combine(left, right);
  PairScore score = make_score(5);
  int class_total = finish_score(score);
  std::cout << "points=" << point_total << std::endl;
  std::cout << "class=" << class_total << std::endl;
  return point_total + class_total;
}
