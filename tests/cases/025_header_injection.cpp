#include "header_adder.h"
#include "header_adder.h"
#include "header_printer.h"

#include <vector>

int main() {
  std::vector<int> values;
  HeaderAdder add_three(3);
  HeaderAdder subtract_one(-1);
  HeaderPrinter printer(8);
  add_three.add(values, 4);
  subtract_one.add(values, 7);
  printer.print(values);
  return values[values.size() - 1] + printer.tag();
}
