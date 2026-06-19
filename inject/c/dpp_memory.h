#pragma once

#include <stddef.h>

typedef void (*dpp_destructor_fn)(void *);

typedef struct dpp_shared_control {
  void *ptr;
  size_t refs;
  dpp_destructor_fn destroy;
} dpp_shared_control;

typedef struct dpp_shared_ptr {
  dpp_shared_control *control;
} dpp_shared_ptr;

void *dpp_unique_new(size_t size);
void dpp_unique_destroy(void *ptr, dpp_destructor_fn destroy);

void *dpp_shared_create(dpp_shared_ptr *target, size_t size, dpp_destructor_fn destroy);
void dpp_shared_copy(dpp_shared_ptr *target, const dpp_shared_ptr *source);
void dpp_shared_destroy(dpp_shared_ptr *ptr);
void *dpp_shared_get(const dpp_shared_ptr *ptr);
