#include <iostream>
#include <memory>
#include <string>
#include <vector>

class Box {
public:
  int value;
  Box(int start) : value(start) {}
  void add(int amount) { value += amount; }
  int get() const { return value; }
};

int choose_value(int pick_first) {
  if (pick_first) {
    std::unique_ptr<Box> box(new Box(3));
    std::vector<int> values;
    std::string label = "cleanup";
    box->add(4);
    values.push_back(box->get());
    values.push_back(label.size());
    return values[0] + values[1];
  }

  std::string fallback = "fallback";
  return fallback.size();
}

int main() {
  int first = choose_value(1);
  int second = choose_value(0);
  std::cout << "cleanup=" << first << "," << second << std::endl;
  return first + second;
}
