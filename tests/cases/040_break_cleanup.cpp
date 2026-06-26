#include <iostream>
#include <memory>
#include <string>

class Counter {
public:
  int value;
  Counter(int v) : value(v) {}
  int get() const { return value; }
};

int main() {
  int total = 0;
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<Counter> c(new Counter(i));
    std::string label = "iter";
    if (i == 3) {
      break;
    }
    total += c->get();
  }
  std::cout << total << std::endl;
  return total;
}
