#include <iostream>

class Left {
public:
  int left;
};

class Right {
public:
  int right;
};

class Combined : public Left, public Right {
public:
  int extra;
};

int main() {
  return 0;
}
