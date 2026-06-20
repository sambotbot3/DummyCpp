#include "score_adder.h"
#include "score_printer.h"

#include <vector>

int run_scores() {
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
