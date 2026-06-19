#include <algorithm>
#include <iostream>
#include <vector>

int main() {
  std::vector<int> xs;
  xs.push_back(4);
  xs.push_back(1);
  xs.push_back(3);
  xs.push_back(2);
  std::sort(xs.begin(), xs.end());
  std::cout << xs[0] << "," << xs[1] << "," << xs[2] << "," << xs[3] << std::endl;
  return 0;
}
