#include <iostream>
#include <map>
#include <unordered_map>

int run_counts() {
  std::map<int, int> ordered;
  std::unordered_map<int, int> hashed;
  ordered[3] = 7;
  ordered[1] = ordered[3] + 2;
  hashed[4] = 5;
  hashed[4] = hashed[4] + ordered[1];
  int total = ordered[1] + hashed[4];
  std::cout << "ordered=" << ordered.size() << std::endl;
  std::cout << "hashed=" << hashed.size() << std::endl;
  std::cout << "count_total=" << total << std::endl;
  return total;
}
