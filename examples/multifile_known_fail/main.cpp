#include "inventory.h"
#include "reporting.h"

#include <iostream>

int main() {
  const InventoryByName inventory = build_inventory();
  const int report_score = print_inventory_report(inventory);
  std::cout << "score=" << report_score << std::endl;
  return report_score == 3 ? 0 : 1;
}
