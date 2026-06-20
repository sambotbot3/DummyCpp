#include <iostream>
#include <map>

int main() {
  std::map<int, int> counts;
  counts[2] = 4;
  counts[5] = counts[2] + 3;
  counts[2] = counts[5] - 1;
  std::cout << "size=" << counts.size() << std::endl;
  std::cout << "value=" << counts[2] + counts[5] << std::endl;
  return counts.size();
}
