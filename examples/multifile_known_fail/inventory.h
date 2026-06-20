#pragma once

#include <map>
#include <string>
#include <vector>

#include "templates.h"

struct Item {
  std::string name;
  int quantity;
  int price_cents;
};

typedef std::map<std::string, Item> InventoryByName;
typedef std::map<std::string, int> TotalsByCategory;

Item make_item(const std::string &name, int quantity, int price_cents);
InventoryByName build_inventory();
TotalsByCategory summarize_by_category(const InventoryByName &inventory);
int inventory_value_cents(const InventoryByName &inventory);
std::vector<Item> expensive_items(const InventoryByName &inventory, int min_price_cents);
