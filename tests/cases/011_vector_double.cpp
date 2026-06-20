#include <iostream>
#include <vector>

int main() {
  std::vector<double> xs;
  xs.push_back(1.5);
  xs.push_back(2.25);
  std::cout << "size=" << xs.size() << std::endl;
  std::cout << "sum=" << xs[0] + xs[1] << std::endl;
  return xs.size();
}
