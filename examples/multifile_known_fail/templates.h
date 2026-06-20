#pragma once

#include <map>
#include <string>

template <typename T>
T clamp_to_zero(T value) {
  return value < static_cast<T>(0) ? static_cast<T>(0) : value;
}

template <typename Key, typename Value>
void add_to_total(std::map<Key, Value> &totals, const Key &key, const Value &amount) {
  totals[key] = totals[key] + amount;
}

template <typename Record>
int line_value_cents(const Record &record) {
  return clamp_to_zero(record.quantity) * clamp_to_zero(record.price_cents);
}
