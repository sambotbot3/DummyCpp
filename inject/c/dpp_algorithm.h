#pragma once

#include "dpp_vector.h"

#include <stddef.h>
#include <stdlib.h>
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

static inline int dpp_algorithm_compare_char(const void *left, const void *right) {
  const char left_value = *(const char *)left;
  const char right_value = *(const char *)right;
  return (left_value > right_value) - (left_value < right_value);
}

static inline int dpp_algorithm_compare_int(const void *left, const void *right) {
  const int left_value = *(const int *)left;
  const int right_value = *(const int *)right;
  return (left_value > right_value) - (left_value < right_value);
}

static inline int dpp_algorithm_compare_long(const void *left, const void *right) {
  const long left_value = *(const long *)left;
  const long right_value = *(const long *)right;
  return (left_value > right_value) - (left_value < right_value);
}

static inline int dpp_algorithm_compare_float(const void *left, const void *right) {
  const float left_value = *(const float *)left;
  const float right_value = *(const float *)right;
  return (left_value > right_value) - (left_value < right_value);
}

static inline int dpp_algorithm_compare_double(const void *left, const void *right) {
  const double left_value = *(const double *)left;
  const double right_value = *(const double *)right;
  return (left_value > right_value) - (left_value < right_value);
}

static inline int dpp_algorithm_compare_char_ptr(const void *left, const void *right) {
  const char *left_value = *(const char *const *)left;
  const char *right_value = *(const char *const *)right;
  return strcmp(left_value, right_value);
}

#define DPP_SORT_VECTOR(vector_ptr, type, compare_fn)                                            \
  do {                                                                                           \
    dpp_vector *dpp_sort_vector = (vector_ptr);                                                   \
    qsort(dpp_sort_vector->data, dpp_sort_vector->size, sizeof(type), (compare_fn));             \
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

static inline int dpp_find_int(const dpp_vector *v, int value) {
  const int *data = (const int *)v->data;
  for (size_t i = 0; i < v->size; ++i) if (data[i] == value) return 1;
  return 0;
}

static inline int dpp_find_double(const dpp_vector *v, double value) {
  const double *data = (const double *)v->data;
  for (size_t i = 0; i < v->size; ++i) if (data[i] == value) return 1;
  return 0;
}

static inline int dpp_count_int(const dpp_vector *v, int value) {
  int count = 0;
  const int *data = (const int *)v->data;
  for (size_t i = 0; i < v->size; ++i) if (data[i] == value) ++count;
  return count;
}

static inline int dpp_count_double(const dpp_vector *v, double value) {
  int count = 0;
  const double *data = (const double *)v->data;
  for (size_t i = 0; i < v->size; ++i) if (data[i] == value) ++count;
  return count;
}

static inline int dpp_accumulate_int(const dpp_vector *v, int init) {
  int acc = init;
  const int *data = (const int *)v->data;
  for (size_t i = 0; i < v->size; ++i) acc += data[i];
  return acc;
}

static inline double dpp_accumulate_double(const dpp_vector *v, double init) {
  double acc = init;
  const double *data = (const double *)v->data;
  for (size_t i = 0; i < v->size; ++i) acc += data[i];
  return acc;
}

static inline int dpp_min_element_int(const dpp_vector *v) {
  const int *data = (const int *)v->data;
  int m = data[0];
  for (size_t i = 1; i < v->size; ++i) if (data[i] < m) m = data[i];
  return m;
}

static inline double dpp_min_element_double(const dpp_vector *v) {
  const double *data = (const double *)v->data;
  double m = data[0];
  for (size_t i = 1; i < v->size; ++i) if (data[i] < m) m = data[i];
  return m;
}

static inline int dpp_max_element_int(const dpp_vector *v) {
  const int *data = (const int *)v->data;
  int m = data[0];
  for (size_t i = 1; i < v->size; ++i) if (data[i] > m) m = data[i];
  return m;
}

static inline double dpp_max_element_double(const dpp_vector *v) {
  const double *data = (const double *)v->data;
  double m = data[0];
  for (size_t i = 1; i < v->size; ++i) if (data[i] > m) m = data[i];
  return m;
}

#define DPP_FILL_VECTOR(vector_ptr, type, value)                                                 \
  do {                                                                                           \
    dpp_vector *dpp_fill_vector = (vector_ptr);                                                   \
    type *dpp_fill_data = (type *)dpp_fill_vector->data;                                         \
    type dpp_fill_value = (value);                                                               \
    for (size_t dpp_fill_i = 0; dpp_fill_i < dpp_fill_vector->size; ++dpp_fill_i) {              \
      dpp_fill_data[dpp_fill_i] = dpp_fill_value;                                                \
    }                                                                                            \
  } while (0)
