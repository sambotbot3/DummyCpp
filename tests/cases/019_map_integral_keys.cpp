#include <iostream>
#include <map>
#include <unordered_map>

int main() {
  std::map<char, int> letters;
  std::unordered_map<long, int> totals;
  letters['a'] = 2;
  letters['b'] = letters['a'] + 3;
  totals[10000000000L] = letters['b'];
  totals[7L] = totals[10000000000L] + 1;
  std::cout << "letters=" << letters.size() << std::endl;
  std::cout << "totals=" << totals.size() << std::endl;
  std::cout << "value=" << letters['b'] + totals[7L] << std::endl;
  return letters.size() + totals.size();
}
