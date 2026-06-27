#pragma once

#include <stddef.h>

typedef int (*dpp_map_compare_fn)(const void *left, const void *right);
typedef size_t (*dpp_map_hash_fn)(const void *key);
typedef int (*dpp_map_equal_fn)(const void *left, const void *right);

typedef struct dpp_map {
  void *entries;
  size_t key_size;
  size_t value_size;
  size_t size;
  size_t capacity;
  dpp_map_compare_fn compare;
} dpp_map;

typedef struct dpp_unordered_map {
  void *entries;
  size_t key_size;
  size_t value_size;
  size_t size;
  size_t capacity;
  dpp_map_hash_fn hash;
  dpp_map_equal_fn equal;
} dpp_unordered_map;

int dpp_compare_long(const void *left, const void *right);
size_t dpp_hash_long(const void *key);
int dpp_equal_long(const void *left, const void *right);
int dpp_compare_cstr(const void *left, const void *right);
size_t dpp_hash_cstr(const void *key);
int dpp_equal_cstr(const void *left, const void *right);

void dpp_map_init(dpp_map *map, size_t key_size, size_t value_size, dpp_map_compare_fn compare);
void dpp_map_destroy(dpp_map *map);
size_t dpp_map_size(const dpp_map *map);
void *dpp_map_get_or_insert(dpp_map *map, const void *key);
const void *dpp_map_find(const dpp_map *map, const void *key);
const void *dpp_unordered_map_find(const dpp_unordered_map *map, const void *key);
int dpp_map_contains(const dpp_map *map, const void *key);
int dpp_map_erase(dpp_map *map, const void *key);
const void *dpp_map_key_at(const dpp_map *map, size_t index);
void *dpp_map_value_at(dpp_map *map, size_t index);
const void *dpp_map_const_value_at(const dpp_map *map, size_t index);

void dpp_unordered_map_init(dpp_unordered_map *map, size_t key_size, size_t value_size,
                            dpp_map_hash_fn hash, dpp_map_equal_fn equal);
void dpp_unordered_map_destroy(dpp_unordered_map *map);
size_t dpp_unordered_map_size(const dpp_unordered_map *map);
void *dpp_unordered_map_get_or_insert(dpp_unordered_map *map, const void *key);
int dpp_unordered_map_contains(const dpp_unordered_map *map, const void *key);
int dpp_unordered_map_erase(dpp_unordered_map *map, const void *key);
const void *dpp_unordered_map_key_at(const dpp_unordered_map *map, size_t index);
void *dpp_unordered_map_value_at(dpp_unordered_map *map, size_t index);
const void *dpp_unordered_map_const_value_at(const dpp_unordered_map *map, size_t index);
