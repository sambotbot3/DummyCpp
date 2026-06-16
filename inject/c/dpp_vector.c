#include "dpp_vector.h"

#include <stdlib.h>

void dpp_vector_init(dpp_vector *vector) {
  vector->data = NULL;
  vector->size = 0;
  vector->capacity = 0;
}

void dpp_vector_destroy(dpp_vector *vector) {
  free(vector->data);
  vector->data = NULL;
  vector->size = 0;
  vector->capacity = 0;
}
