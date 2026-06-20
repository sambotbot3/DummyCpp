#include <algorithm>
#include <iostream>
#include <vector>

int main() {
  std::vector<int> xs;
  xs.push_back(1);
  xs.push_back(2);
  xs.push_back(3);
  std::reverse(xs.begin(), xs.end());
  std::fill(xs.begin(), xs.end(), std::max(4, std::min(9, 5)));
  int left = xs[0];
  int right = 7;
  std::swap(left, right);
  std::cout << xs[0] << "," << xs[1] << "," << xs[2] << "," << left << "," << right << std::endl;
  return 0;
}
