#include "dpp_string.h"
#include "dpp_vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dpp_string_abort_oom(void) {
  fputs("dpp_string: allocation failed\n", stderr);
  abort();
}

static void dpp_string_grow(dpp_string *s, size_t min_cap) {
  size_t next = s->capacity == 0 ? 8 : s->capacity * 2;
  if (next < min_cap) next = min_cap;
  char *data = (char *)realloc(s->data, next + 1);
  if (data == NULL) dpp_string_abort_oom();
  s->data = data;
  s->capacity = next;
}

static char *dpp_string_copy_cstr(const char *value, size_t *size, size_t *capacity) {
  const char *safe_value = value == NULL ? "" : value;
  const size_t length = strlen(safe_value);
  char *copy = (char *)malloc(length + 1);
  if (copy == NULL) {
    dpp_string_abort_oom();
  }
  memcpy(copy, safe_value, length + 1);
  *size = length;
  *capacity = length;
  return copy;
}

void dpp_string_init(dpp_string *string) {
  dpp_string_init_cstr(string, "");
}

void dpp_string_init_cstr(dpp_string *string, const char *value) {
  string->data = dpp_string_copy_cstr(value, &string->size, &string->capacity);
}

void dpp_string_init_fill(dpp_string *string, size_t count, char c) {
  char *buf = (char *)malloc(count + 1);
  if (buf == NULL) dpp_string_abort_oom();
  memset(buf, (unsigned char)c, count);
  buf[count] = '\0';
  string->data = buf;
  string->size = count;
  string->capacity = count;
}

void dpp_string_assign_cstr(dpp_string *string, const char *value) {
  char *copy = dpp_string_copy_cstr(value, &string->size, &string->capacity);
  free(string->data);
  string->data = copy;
}

void dpp_string_append_cstr(dpp_string *string, const char *value) {
  const char *suffix = value == NULL ? "" : value;
  const size_t suffix_size = strlen(suffix);
  const size_t needed = string->size + suffix_size;
  if (needed > string->capacity) {
    dpp_string_grow(string, needed);
  }
  memcpy(string->data + string->size, suffix, suffix_size + 1);
  string->size += suffix_size;
}

void dpp_string_destroy(dpp_string *string) {
  free(string->data);
  string->data = NULL;
  string->size = 0;
  string->capacity = 0;
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

void dpp_string_push_back(dpp_string *s, char c) {
  char buf[2] = {c, '\0'};
  dpp_string_append_cstr(s, buf);
}

size_t dpp_string_find_cstr(const dpp_string *s, const char *needle) {
  const char *result = strstr(dpp_string_c_str(s), needle == NULL ? "" : needle);
  return result ? (size_t)(result - s->data) : (size_t)-1;
}

size_t dpp_string_find_cstr_from(const dpp_string *s, const char *needle, size_t pos) {
  const char *data = dpp_string_c_str(s);
  if (pos > s->size) return (size_t)-1;
  const char *result = strstr(data + pos, needle == NULL ? "" : needle);
  return result ? (size_t)(result - data) : (size_t)-1;
}

static dpp_string dpp_string_from_buf(char *buf, size_t len) {
  dpp_string result;
  result.data = buf;
  result.size = len;
  result.capacity = len;
  return result;
}

dpp_string dpp_string_from_int(int n) {
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%d", n);
  char *data = (char *)malloc((size_t)len + 1);
  if (!data) { fputs("dpp_string: allocation failed\n", stderr); abort(); }
  memcpy(data, buf, (size_t)len + 1);
  return dpp_string_from_buf(data, (size_t)len);
}

dpp_string dpp_string_from_long(long n) {
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%ld", n);
  char *data = (char *)malloc((size_t)len + 1);
  if (!data) { fputs("dpp_string: allocation failed\n", stderr); abort(); }
  memcpy(data, buf, (size_t)len + 1);
  return dpp_string_from_buf(data, (size_t)len);
}

dpp_string dpp_string_from_double(double n) {
  char buf[64];
  const int len = snprintf(buf, sizeof(buf), "%.6f", n);
  char *data = (char *)malloc((size_t)len + 1);
  if (!data) { fputs("dpp_string: allocation failed\n", stderr); abort(); }
  memcpy(data, buf, (size_t)len + 1);
  return dpp_string_from_buf(data, (size_t)len);
}

dpp_string dpp_string_substr(const dpp_string *s, size_t pos, size_t len) {
  dpp_string result;
  const size_t src_size = s->size;
  if (pos > src_size) pos = src_size;
  if (len > src_size - pos) len = src_size - pos;
  result.data = (char *)malloc(len + 1);
  if (result.data == NULL) {
    fputs("dpp_string: allocation failed\n", stderr);
    abort();
  }
  memcpy(result.data, s->data + pos, len);
  result.data[len] = '\0';
  result.size = len;
  result.capacity = len;
  return result;
}

void dpp_vector_destroy_strings(dpp_vector *v) {
  size_t i;
  for (i = 0; i < dpp_vector_size(v); ++i) {
    dpp_string_destroy((dpp_string *)dpp_vector_at(v, i));
  }
  dpp_vector_destroy(v);
}

void dpp_string_vector_push(dpp_vector *v, const dpp_string *s) {
  dpp_string copy;
  dpp_string_init_cstr(&copy, dpp_string_c_str(s));
  dpp_vector_push_back(v, &copy);
}

void dpp_string_vector_push_cstr(dpp_vector *v, const char *s) {
  dpp_string tmp;
  dpp_string_init_cstr(&tmp, s);
  dpp_vector_push_back(v, &tmp);
}
