#include <iostream>
#include <vector>

int main() {
  std::vector<int> xs;
  xs.push_back(2);
  xs.push_back(5);
  std::cout << "size=" << xs.size() << std::endl;
  std::cout << "sum=" << xs[0] + xs[1] << std::endl;
  return xs.size();
}
