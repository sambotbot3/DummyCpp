#include <iostream>

class Left {
public:
  int left;
  Left(int value) : left(value) {}
  int left_value() const { return left; }
};

class Right {
public:
  int right;
  Right(int value) : right(value) {}
};

class Combined : private Left, protected Right {
public:
  int extra;
  Combined(int left_value, int right_value, int extra_value) : Left(left_value), Right(right_value), extra(extra_value) {}
  int total() const { return left + right + extra; }
};

int main() {
  Combined combined(2, 3, 5);
  std::cout << "total=" << combined.total() << std::endl;
  return combined.total();
}
