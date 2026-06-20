#include "dpp_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dpp_string_abort_oom(void) {
  fputs("dpp_string: allocation failed\n", stderr);
  abort();
}

static char *dpp_string_copy_cstr(const char *value, size_t *size) {
  const char *safe_value = value == NULL ? "" : value;
  const size_t length = strlen(safe_value);
  char *copy = (char *)malloc(length + 1);
  if (copy == NULL) {
    dpp_string_abort_oom();
  }
  memcpy(copy, safe_value, length + 1);
  *size = length;
  return copy;
}

void dpp_string_init(dpp_string *string) {
  dpp_string_init_cstr(string, "");
}

void dpp_string_init_cstr(dpp_string *string, const char *value) {
  string->data = dpp_string_copy_cstr(value, &string->size);
}

void dpp_string_assign_cstr(dpp_string *string, const char *value) {
  char *copy = dpp_string_copy_cstr(value, &string->size);
  free(string->data);
  string->data = copy;
}

void dpp_string_append_cstr(dpp_string *string, const char *value) {
  const char *suffix = value == NULL ? "" : value;
  const size_t suffix_size = strlen(suffix);
  char *data = (char *)realloc(string->data, string->size + suffix_size + 1);
  if (data == NULL) {
    dpp_string_abort_oom();
  }
  memcpy(data + string->size, suffix, suffix_size + 1);
  string->data = data;
  string->size += suffix_size;
}

void dpp_string_destroy(dpp_string *string) {
  free(string->data);
  string->data = NULL;
  string->size = 0;
}

size_t dpp_string_size(const dpp_string *string) {
  return string->size;
}

const char *dpp_string_c_str(const dpp_string *string) {
  return string->data == NULL ? "" : string->data;
}

int dpp_string_equal(const dpp_string *left, const dpp_string *right) {
  return strcmp(dpp_string_c_str(left), dpp_string_c_str(right)) == 0;
}

int dpp_string_equal_cstr(const dpp_string *left, const char *right) {
  const char *safe_right = right == NULL ? "" : right;
  return strcmp(dpp_string_c_str(left), safe_right) == 0;
}

int dpp_string_compare(const dpp_string *left, const dpp_string *right) {
  return strcmp(dpp_string_c_str(left), dpp_string_c_str(right));
}

int dpp_string_compare_cstr(const dpp_string *left, const char *right) {
  const char *safe_right = right == NULL ? "" : right;
  return strcmp(dpp_string_c_str(left), safe_right);
}
