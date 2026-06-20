#pragma once

#include <stddef.h>

typedef struct dpp_vector {
  void *data;
  size_t elem_size;
  size_t size;
  size_t capacity;
} dpp_vector;

void dpp_vector_init(dpp_vector *vector, size_t elem_size);
void dpp_vector_destroy(dpp_vector *vector);
void dpp_vector_clear(dpp_vector *vector);
void dpp_vector_reserve(dpp_vector *vector, size_t capacity);
void dpp_vector_resize(dpp_vector *vector, size_t size);
void dpp_vector_resize_fill(dpp_vector *vector, size_t size, const void *elem);
size_t dpp_vector_size(const dpp_vector *vector);
void *dpp_vector_at(dpp_vector *vector, size_t index);
const void *dpp_vector_const_at(const dpp_vector *vector, size_t index);
void dpp_vector_push_back(dpp_vector *vector, const void *elem);
void dpp_vector_pop_back(dpp_vector *vector);
