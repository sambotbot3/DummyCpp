#include <iostream>
#include <string>

int main() {
  std::string label = "alpha";
  std::string copy(label);
  int score = 0;
  if (label == copy) {
    score += 3;
  }
  if (label == "alpha") {
    score += 5;
  }
  if ("beta" != label) {
    score += 7;
  }
  copy = "beta";
  if (label != copy) {
    score += 11;
  }
  std::cout << "score=" << score << std::endl;
  return score;
}
