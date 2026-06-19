#include <algorithm>
#include <iostream>
#include <vector>

int main() {
  std::vector<int> xs;
  xs.push_back(5);
  xs.push_back(2);
  xs.push_back(8);
  std::sort(xs.begin(), xs.end());
  std::cout << "smallest=" << xs[0] << " largest=" << xs[2] << std::endl;
  return 0;
}
