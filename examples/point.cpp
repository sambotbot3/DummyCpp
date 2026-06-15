struct Point {
  int x;
  int y;
};

int add(int a, int b) {
  return a + b;
}

int main() {
  Point p{1, 2};
  return add(p.x, p.y);
}

