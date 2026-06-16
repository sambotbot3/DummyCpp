#include "dpp_vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dpp_vector_abort_oom(void) {
  fputs("dpp_vector: allocation failed\n", stderr);
  abort();
}

static void dpp_vector_abort_bounds(void) {
  fputs("dpp_vector: index out of bounds\n", stderr);
  abort();
}

static void dpp_vector_reserve(dpp_vector *vector, size_t capacity) {
  if (capacity <= vector->capacity) {
    return;
  }
  void *data = realloc(vector->data, capacity * vector->elem_size);
  if (data == NULL) {
    dpp_vector_abort_oom();
  }
  vector->data = data;
  vector->capacity = capacity;
}

void dpp_vector_init(dpp_vector *vector, size_t elem_size) {
  vector->data = NULL;
  vector->elem_size = elem_size;
  vector->size = 0;
  vector->capacity = 0;
}

void dpp_vector_destroy(dpp_vector *vector) {
  free(vector->data);
  vector->data = NULL;
  vector->elem_size = 0;
  vector->size = 0;
  vector->capacity = 0;
}

size_t dpp_vector_size(const dpp_vector *vector) {
  return vector->size;
}

void *dpp_vector_at(dpp_vector *vector, size_t index) {
  if (index >= vector->size) {
    dpp_vector_abort_bounds();
  }
  return (char *)vector->data + index * vector->elem_size;
}

const void *dpp_vector_const_at(const dpp_vector *vector, size_t index) {
  if (index >= vector->size) {
    dpp_vector_abort_bounds();
  }
  return (const char *)vector->data + index * vector->elem_size;
}

void dpp_vector_push_back(dpp_vector *vector, const void *elem) {
  if (vector->size == vector->capacity) {
    const size_t next_capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
    dpp_vector_reserve(vector, next_capacity);
  }
  memcpy((char *)vector->data + vector->size * vector->elem_size, elem, vector->elem_size);
  vector->size += 1;
}
