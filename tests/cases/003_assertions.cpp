#include <cassert>
#include <iostream>

int main() {
  int value = 4 * 3;
  assert(value == 12);
  std::cout << "asserted=" << value << std::endl;
  return 0;
}
