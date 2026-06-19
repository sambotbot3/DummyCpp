#include <iostream>
#include <unordered_map>

int main() {
  std::unordered_map<int, int> counts;
  counts[10] = 1;
  counts[7] = counts[10] + 6;
  counts[10] = counts[7] + counts[10];
  std::cout << "size=" << counts.size() << std::endl;
  std::cout << "value=" << counts[10] + counts[7] << std::endl;
  return counts.size();
}
