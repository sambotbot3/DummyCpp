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

int first_pair_sum() {
  std::vector<int> xs;
  xs.push_back(4);
  xs.push_back(9);
  std::cout << "helper_size=" << xs.size() << std::endl;
  return xs[0] + xs[1];
}

int vector_scores() {
  std::vector<PairScore> scores;
  PairScore first = make_score(2);
  scores.push_back(first);
  scores.push_back(PairScore{5, 8});
  int total = 0;
  for (int i = 0; i < 2; i = i + 1) {
    total = total + scores[i].total();
  }
  std::cout << "scores=" << scores.size() << std::endl;
  std::cout << "score_total=" << total << std::endl;
  return total;
}

int choose_value(int pick_second) {
  std::vector<int> xs;
  xs.push_back(11);
  xs.push_back(17);
  if (pick_second) {
    std::cout << "chosen=" << xs[1] << std::endl;
    return xs[1];
  }
  std::cout << "chosen=" << xs[0] << std::endl;
  return xs[0];
}

int main() {
  int total = first_pair_sum() + vector_scores() + choose_value(1);
  std::cout << "all_vectors=" << total << std::endl;
  return total;
}
