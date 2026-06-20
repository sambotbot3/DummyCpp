#include <iostream>
#include <vector>

class VectorRole {
public:
  int amount;
  VectorRole(int start) : amount(start) {}
  int tag() const { return amount; }
};

class ScoreAdder : public VectorRole {
public:
  ScoreAdder(int start) : VectorRole(start) {}
  void add(std::vector<int> &values, int value) { values.push_back(value + amount); }
};

class ScorePrinter : public VectorRole {
public:
  ScorePrinter(int start) : VectorRole(start) {}
  void print(const std::vector<int> &values) const { std::cout << "size=" << values.size() << " last=" << values[values.size() - 1] << " tag=" << amount << std::endl; }
};

int main() {
  std::vector<int> scores;
  ScoreAdder bonus(2);
  ScoreAdder penalty(-1);
  ScorePrinter printer(9);
  bonus.add(scores, 3);
  bonus.add(scores, 5);
  penalty.add(scores, 11);
  printer.print(scores);
  return scores[scores.size() - 1] + printer.tag();
}
