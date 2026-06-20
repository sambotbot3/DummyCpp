#include <cassert>
#include <iostream>
#include <string>

int main() {
  std::string low = "alpha";
  std::string mid = "bravo";
  std::string high = "charlie";

  assert(low < mid);
  assert(high > mid);
  assert(low <= "alpha");
  assert(mid >= "bravo");
  assert("alpha" <= low);
  assert("delta" > high);

  int score = 0;
  if (low < mid && mid < high) {
    score += 3;
  }
  if (high >= "charlie" && "bravo" >= mid) {
    score += 4;
  }

  std::cout << "order=" << score << " first=" << low.c_str() << std::endl;
  return score == 7 ? 0 : 1;
}
