#include "dpp_memory.h"

#include <stdio.h>
#include <stdlib.h>

static void dpp_memory_abort_oom(void) {
  fputs("dpp_memory: allocation failed\n", stderr);
  abort();
}

void *dpp_unique_new(size_t size) {
  void *ptr = calloc(1, size);
  if (ptr == NULL) {
    dpp_memory_abort_oom();
  }
  return ptr;
}

void dpp_unique_destroy(void *ptr, dpp_destructor_fn destroy) {
  if (ptr == NULL) {
    return;
  }
  if (destroy != NULL) {
    destroy(ptr);
  }
  free(ptr);
}

void *dpp_shared_create(dpp_shared_ptr *target, size_t size, dpp_destructor_fn destroy) {
  dpp_shared_control *control = (dpp_shared_control *)calloc(1, sizeof(dpp_shared_control));
  if (control == NULL) {
    dpp_memory_abort_oom();
  }

  control->ptr = calloc(1, size);
  if (control->ptr == NULL) {
    free(control);
    dpp_memory_abort_oom();
  }

  control->refs = 1;
  control->destroy = destroy;
  target->control = control;
  return control->ptr;
}

void dpp_shared_copy(dpp_shared_ptr *target, const dpp_shared_ptr *source) {
  target->control = source->control;
  if (target->control != NULL) {
    target->control->refs += 1;
  }
}

void dpp_shared_destroy(dpp_shared_ptr *ptr) {
  if (ptr->control == NULL) {
    return;
  }

  ptr->control->refs -= 1;
  if (ptr->control->refs == 0) {
    if (ptr->control->destroy != NULL) {
      ptr->control->destroy(ptr->control->ptr);
    }
    free(ptr->control->ptr);
    free(ptr->control);
  }
  ptr->control = NULL;
}

void *dpp_shared_get(const dpp_shared_ptr *ptr) {
  return ptr->control == NULL ? NULL : ptr->control->ptr;
}
