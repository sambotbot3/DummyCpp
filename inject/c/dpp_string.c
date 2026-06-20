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
