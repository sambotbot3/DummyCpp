#include <iostream>

int accumulate_until(int limit) {
  int total = 0;
  int i = 0;
  while (i < limit) {
    i = i + 1;
    if (i == 2) {
      continue;
    } else {
      total = total + i;
    }
    if (total > 7) {
      break;
    }
  }
  do {
    total = total + 1;
  } while (total < 10);
  return total;
}

int main() {
  int value = accumulate_until(6);
  std::cout << "flow=" << value << std::endl;
  return value;
}
