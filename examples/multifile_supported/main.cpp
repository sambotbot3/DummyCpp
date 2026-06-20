#include <iostream>

int run_scores();
int run_counts();
int run_ordering();

int main() {
  int total = run_scores() + run_counts() + run_ordering();
  std::cout << "grand=" << total << std::endl;
  return total;
}
