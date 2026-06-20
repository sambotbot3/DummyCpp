#include <iostream>
#include <vector>

int main() {
  std::vector<int> values;
  values.reserve(8);
  values.resize(3);
  values[0] = 4;
  values[1] = values.front() + 5;
  values[2] = values.back() + 6;
  values.pop_back();
  values.push_back(values.front() + values.back());
  values.resize(5, 2);
  std::cout << "size=" << values.size() << std::endl;
  std::cout << "front=" << values.front() << " back=" << values.back() << std::endl;
  return values.front() + values.back() + values[3];
}
