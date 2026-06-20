#include <iostream>
#include <map>
#include <string>

int main() {
  std::map<std::string, int> counts;
  counts["apple"] = 2;
  counts["banana"] = counts["apple"] + 3;
  counts["apple"] = counts["banana"] + 1;
  std::cout << "size=" << counts.size() << std::endl;
  std::cout << "value=" << counts["apple"] + counts["banana"] << std::endl;
  return counts.size();
}
