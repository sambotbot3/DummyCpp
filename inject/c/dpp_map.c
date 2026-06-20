#include "dpp_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct dpp_map_entry {
  unsigned char *key;
  unsigned char *value;
} dpp_map_entry;

typedef struct dpp_unordered_map_entry {
  unsigned char *key;
  unsigned char *value;
  int occupied;
} dpp_unordered_map_entry;

static void dpp_map_abort_oom(void) {
  fputs("dpp_map: allocation failed\n", stderr);
  abort();
}

static dpp_map_entry *dpp_map_entries(dpp_map *map) {
  return (dpp_map_entry *)map->entries;
}

static dpp_unordered_map_entry *dpp_unordered_map_entries(dpp_unordered_map *map) {
  return (dpp_unordered_map_entry *)map->entries;
}

static unsigned char *dpp_copy_bytes(const void *source, size_t size) {
  unsigned char *copy = (unsigned char *)malloc(size);
  if (copy == NULL) {
    dpp_map_abort_oom();
  }
  memcpy(copy, source, size);
  return copy;
}

static unsigned char *dpp_zero_bytes(size_t size) {
  unsigned char *copy = (unsigned char *)calloc(1, size);
  if (copy == NULL) {
    dpp_map_abort_oom();
  }
  return copy;
}

static void dpp_map_reserve(dpp_map *map, size_t capacity) {
  if (capacity <= map->capacity) {
    return;
  }
  dpp_map_entry *entries = (dpp_map_entry *)realloc(map->entries, capacity * sizeof(dpp_map_entry));
  if (entries == NULL) {
    dpp_map_abort_oom();
  }
  map->entries = entries;
  map->capacity = capacity;
}

static void dpp_unordered_map_rehash(dpp_unordered_map *map, size_t capacity) {
  dpp_unordered_map_entry *old_entries = dpp_unordered_map_entries(map);
  const size_t old_capacity = map->capacity;
  dpp_unordered_map_entry *entries =
      (dpp_unordered_map_entry *)calloc(capacity, sizeof(dpp_unordered_map_entry));
  if (entries == NULL) {
    dpp_map_abort_oom();
  }

  map->entries = entries;
  map->capacity = capacity;
  map->size = 0;

  for (size_t index = 0; index < old_capacity; ++index) {
    if (!old_entries[index].occupied) {
      continue;
    }
    size_t slot = map->hash(old_entries[index].key) % map->capacity;
    while (entries[slot].occupied) {
      slot = (slot + 1) % map->capacity;
    }
    entries[slot] = old_entries[index];
    map->size += 1;
  }

  free(old_entries);
}

int dpp_compare_long(const void *left, const void *right) {
  const long left_value = *(const long *)left;
  const long right_value = *(const long *)right;
  return (left_value > right_value) - (left_value < right_value);
}

size_t dpp_hash_long(const void *key) {
  return (size_t)*(const long *)key * 11400714819323198485ull;
}

int dpp_equal_long(const void *left, const void *right) {
  return *(const long *)left == *(const long *)right;
}

int dpp_compare_cstr(const void *left, const void *right) {
  const char *left_value = *(const char *const *)left;
  const char *right_value = *(const char *const *)right;
  return strcmp(left_value, right_value);
}

size_t dpp_hash_cstr(const void *key) {
  const unsigned char *text = (const unsigned char *)*(const char *const *)key;
  size_t hash = 1469598103934665603ull;
  while (*text != '\0') {
    hash ^= (size_t)*text;
    hash *= 1099511628211ull;
    ++text;
  }
  return hash;
}

int dpp_equal_cstr(const void *left, const void *right) {
  return dpp_compare_cstr(left, right) == 0;
}

void dpp_map_init(dpp_map *map, size_t key_size, size_t value_size, dpp_map_compare_fn compare) {
  map->entries = NULL;
  map->key_size = key_size;
  map->value_size = value_size;
  map->size = 0;
  map->capacity = 0;
  map->compare = compare;
}

void dpp_map_destroy(dpp_map *map) {
  dpp_map_entry *entries = dpp_map_entries(map);
  for (size_t index = 0; index < map->size; ++index) {
    free(entries[index].key);
    free(entries[index].value);
  }
  free(entries);
  map->entries = NULL;
  map->key_size = 0;
  map->value_size = 0;
  map->size = 0;
  map->capacity = 0;
  map->compare = NULL;
}

size_t dpp_map_size(const dpp_map *map) {
  return map->size;
}

void *dpp_map_get_or_insert(dpp_map *map, const void *key) {
  dpp_map_entry *entries = dpp_map_entries(map);
  size_t pos = 0;
  while (pos < map->size && map->compare(entries[pos].key, key) < 0) {
    ++pos;
  }
  if (pos < map->size && map->compare(entries[pos].key, key) == 0) {
    return entries[pos].value;
  }

  if (map->size == map->capacity) {
    const size_t next_capacity = map->capacity == 0 ? 4 : map->capacity * 2;
    dpp_map_reserve(map, next_capacity);
    entries = dpp_map_entries(map);
  }

  for (size_t index = map->size; index > pos; --index) {
    entries[index] = entries[index - 1];
  }
  entries[pos].key = dpp_copy_bytes(key, map->key_size);
  entries[pos].value = dpp_zero_bytes(map->value_size);
  map->size += 1;
  return entries[pos].value;
}

void dpp_unordered_map_init(dpp_unordered_map *map, size_t key_size, size_t value_size,
                            dpp_map_hash_fn hash, dpp_map_equal_fn equal) {
  map->entries = NULL;
  map->key_size = key_size;
  map->value_size = value_size;
  map->size = 0;
  map->capacity = 0;
  map->hash = hash;
  map->equal = equal;
}

void dpp_unordered_map_destroy(dpp_unordered_map *map) {
  dpp_unordered_map_entry *entries = dpp_unordered_map_entries(map);
  for (size_t index = 0; index < map->capacity; ++index) {
    if (entries[index].occupied) {
      free(entries[index].key);
      free(entries[index].value);
    }
  }
  free(entries);
  map->entries = NULL;
  map->key_size = 0;
  map->value_size = 0;
  map->size = 0;
  map->capacity = 0;
  map->hash = NULL;
  map->equal = NULL;
}

size_t dpp_unordered_map_size(const dpp_unordered_map *map) {
  return map->size;
}

void *dpp_unordered_map_get_or_insert(dpp_unordered_map *map, const void *key) {
  if (map->capacity == 0 || (map->size + 1) * 4 >= map->capacity * 3) {
    const size_t next_capacity = map->capacity == 0 ? 8 : map->capacity * 2;
    dpp_unordered_map_rehash(map, next_capacity);
  }

  dpp_unordered_map_entry *entries = dpp_unordered_map_entries(map);
  size_t slot = map->hash(key) % map->capacity;
  while (entries[slot].occupied) {
    if (map->equal(entries[slot].key, key)) {
      return entries[slot].value;
    }
    slot = (slot + 1) % map->capacity;
  }

  entries[slot].key = dpp_copy_bytes(key, map->key_size);
  entries[slot].value = dpp_zero_bytes(map->value_size);
  entries[slot].occupied = 1;
  map->size += 1;
  return entries[slot].value;
}
