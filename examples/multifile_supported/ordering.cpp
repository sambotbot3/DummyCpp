#include <algorithm>
#include <iostream>
#include <vector>

int run_ordering() {
  std::vector<int> xs;
  xs.push_back(5);
  xs.push_back(2);
  xs.push_back(8);
  std::sort(xs.begin(), xs.end());
  std::reverse(xs.begin(), xs.end());
  int total = xs[0] + xs[1] + xs[2];
  std::cout << "largest=" << xs[0] << std::endl;
  std::cout << "order_total=" << total << std::endl;
  return total;
}
