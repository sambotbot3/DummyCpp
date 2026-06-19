#pragma once

#include "dpp_vector.h"

#include <stddef.h>
#include <string.h>

#define DPP_MIN(left, right) ((left) < (right) ? (left) : (right))
#define DPP_MAX(left, right) ((left) > (right) ? (left) : (right))

#define DPP_SWAP(left, right)                                                                    \
  do {                                                                                           \
    unsigned char dpp_swap_tmp[sizeof(left)];                                                     \
    memcpy(dpp_swap_tmp, &(left), sizeof(left));                                                  \
    memcpy(&(left), &(right), sizeof(left));                                                      \
    memcpy(&(right), dpp_swap_tmp, sizeof(left));                                                 \
  } while (0)

#define DPP_SORT_VECTOR(vector_ptr, type)                                                        \
  do {                                                                                           \
    dpp_vector *dpp_sort_vector = (vector_ptr);                                                   \
    type *dpp_sort_data = (type *)dpp_sort_vector->data;                                         \
    for (size_t dpp_sort_i = 1; dpp_sort_i < dpp_sort_vector->size; ++dpp_sort_i) {              \
      type dpp_sort_value = dpp_sort_data[dpp_sort_i];                                           \
      size_t dpp_sort_j = dpp_sort_i;                                                            \
      while (dpp_sort_j > 0 && dpp_sort_value < dpp_sort_data[dpp_sort_j - 1]) {                 \
        dpp_sort_data[dpp_sort_j] = dpp_sort_data[dpp_sort_j - 1];                               \
        --dpp_sort_j;                                                                            \
      }                                                                                          \
      dpp_sort_data[dpp_sort_j] = dpp_sort_value;                                                \
    }                                                                                            \
  } while (0)

#define DPP_REVERSE_VECTOR(vector_ptr, type)                                                     \
  do {                                                                                           \
    dpp_vector *dpp_reverse_vector = (vector_ptr);                                                \
    type *dpp_reverse_data = (type *)dpp_reverse_vector->data;                                   \
    for (size_t dpp_reverse_i = 0, dpp_reverse_j = dpp_reverse_vector->size;                     \
         dpp_reverse_i < dpp_reverse_j / 2; ++dpp_reverse_i) {                                   \
      type dpp_reverse_tmp = dpp_reverse_data[dpp_reverse_i];                                    \
      dpp_reverse_data[dpp_reverse_i] = dpp_reverse_data[dpp_reverse_j - dpp_reverse_i - 1];     \
      dpp_reverse_data[dpp_reverse_j - dpp_reverse_i - 1] = dpp_reverse_tmp;                     \
    }                                                                                            \
  } while (0)

#define DPP_FILL_VECTOR(vector_ptr, type, value)                                                 \
  do {                                                                                           \
    dpp_vector *dpp_fill_vector = (vector_ptr);                                                   \
    type *dpp_fill_data = (type *)dpp_fill_vector->data;                                         \
    type dpp_fill_value = (value);                                                               \
    for (size_t dpp_fill_i = 0; dpp_fill_i < dpp_fill_vector->size; ++dpp_fill_i) {              \
      dpp_fill_data[dpp_fill_i] = dpp_fill_value;                                                \
    }                                                                                            \
  } while (0)
