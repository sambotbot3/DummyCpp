#include <iostream>
#include <vector>

int main() {
  std::vector<int> values{2, 4, 6};
  int first_empty = values.empty();
  values.push_back(8);
  int before_clear = values.back();
  values.clear();
  int after_clear = values.empty();
  values.push_back(11);
  values.push_back(13);
  std::cout << "first_empty=" << first_empty << std::endl;
  std::cout << "after_clear=" << after_clear << " back=" << values.back() << std::endl;
  return before_clear + values.back();
}
