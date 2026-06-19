#include <iostream>
#include <vector>

class PairScore {
public:
  int left;
  int right;
  PairScore(int a, int b) : left(a), right(b) {}
  void add_left(int value) { left += value; }
  int total() const { return left + right; }
};

PairScore make_score(int base) {
  PairScore score(base, base + 2);
  score.add_left(1);
  return score;
}

int run_scores() {
  std::vector<PairScore> scores;
  PairScore first = make_score(2);
  scores.push_back(first);
  scores.push_back(PairScore{5, 8});
  int total = scores[0].total() + scores[1].total();
  std::cout << "scores=" << scores.size() << std::endl;
  std::cout << "total=" << total << std::endl;
  return total;
}

int main() {
  return run_scores();
}
