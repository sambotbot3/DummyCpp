#include "reporting.h"

#include <iostream>

int print_inventory_report(const InventoryByName &inventory) {
  const TotalsByCategory totals = summarize_by_category(inventory);
  const std::vector<Item> premium_items = expensive_items(inventory, 1000);

  std::cout << "items=" << inventory.size() << std::endl;
  std::cout << "value=" << inventory_value_cents(inventory) << std::endl;

  for (const auto &entry : totals) {
    std::cout << "category " << entry.first << "=" << entry.second << std::endl;
  }

  for (const Item &item : premium_items) {
    std::cout << "premium " << item.name << "=" << item.price_cents << std::endl;
  }

  return static_cast<int>(premium_items.size() + totals.size());
}
