#pragma once

#include <stddef.h>

typedef struct dpp_vector {
  void *data;
  size_t size;
  size_t capacity;
} dpp_vector;

void dpp_vector_init(dpp_vector *vector);
void dpp_vector_destroy(dpp_vector *vector);

