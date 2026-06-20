#include <iostream>

template <typename T>
T clamp_to_zero(T value) {
  return value < static_cast<T>(0) ? static_cast<T>(0) : value;
}

template <typename Left, typename Right>
int weighted_sum(Left left, Right right) {
  return static_cast<int>(left) * 2 + static_cast<int>(right);
}

int main() {
  int negative = -4;
  int clamped = clamp_to_zero(negative);
  int explicit_clamped = clamp_to_zero<int>(7);
  int weighted = weighted_sum<int, long>(3, 5L);
  std::cout << "templates=" << clamped + explicit_clamped + weighted << std::endl;
  return clamped + explicit_clamped + weighted;
}
