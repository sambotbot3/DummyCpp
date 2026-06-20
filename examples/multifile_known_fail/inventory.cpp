#include "inventory.h"

#include <algorithm>

Item make_item(const std::string &name, int quantity, int price_cents) {
  Item item{name, quantity, price_cents};
  return item;
}

InventoryByName build_inventory() {
  InventoryByName inventory;
  inventory["apple"] = make_item("apple", 5, 125);
  inventory["coffee"] = make_item("coffee", 2, 1299);
  inventory["notebook"] = make_item("notebook", 3, 499);
  inventory["coupon"] = make_item("coupon", -4, 100);
  return inventory;
}

TotalsByCategory summarize_by_category(const InventoryByName &inventory) {
  TotalsByCategory totals;
  for (const auto &entry : inventory) {
    const Item &item = entry.second;
    const std::string category = item.price_cents >= 1000 ? "premium" : "basic";
    add_to_total<std::string, int>(totals, category, line_value_cents(item));
  }
  return totals;
}

int inventory_value_cents(const InventoryByName &inventory) {
  int total = 0;
  for (const auto &entry : inventory) {
    total += line_value_cents(entry.second);
  }
  return total;
}

std::vector<Item> expensive_items(const InventoryByName &inventory, int min_price_cents) {
  std::vector<Item> selected;
  for (const auto &entry : inventory) {
    if (entry.second.price_cents >= min_price_cents) {
      selected.push_back(entry.second);
    }
  }

  std::sort(selected.begin(), selected.end(), [](const Item &left, const Item &right) {
    return left.price_cents > right.price_cents;
  });
  return selected;
}
