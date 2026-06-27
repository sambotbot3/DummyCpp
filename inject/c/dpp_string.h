#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct dpp_string {
  char *data;
  size_t size;
  size_t capacity;
} dpp_string;

void dpp_string_init(dpp_string *string);
void dpp_string_init_cstr(dpp_string *string, const char *value);
void dpp_string_init_fill(dpp_string *string, size_t count, char c);
void dpp_string_assign_cstr(dpp_string *string, const char *value);
void dpp_string_append_cstr(dpp_string *string, const char *value);
void dpp_string_destroy(dpp_string *string);
size_t dpp_string_size(const dpp_string *string);
const char *dpp_string_c_str(const dpp_string *string);
int dpp_string_equal(const dpp_string *left, const dpp_string *right);
int dpp_string_equal_cstr(const dpp_string *left, const char *right);
int dpp_string_compare(const dpp_string *left, const dpp_string *right);
int dpp_string_compare_cstr(const dpp_string *left, const char *right);
void dpp_string_push_back(dpp_string *s, char c);
size_t dpp_string_find_cstr(const dpp_string *s, const char *needle);
size_t dpp_string_find_cstr_from(const dpp_string *s, const char *needle, size_t pos);
dpp_string dpp_string_substr(const dpp_string *s, size_t pos, size_t len);
dpp_string dpp_string_from_int(int n);
dpp_string dpp_string_from_long(long n);
dpp_string dpp_string_from_double(double n);
/* C11 _Generic dispatch — used by the dpp_to_string(x) macro */
#define dpp_to_string(x) _Generic((x), \
    int: dpp_string_from_int, \
    long: dpp_string_from_long, \
    double: dpp_string_from_double, \
    float: dpp_string_from_double \
  )(x)

/* Append a by-value dpp_string (takes ownership, destroys src after appending) */
static inline void dpp_string_append_dpp(dpp_string *dst, dpp_string src) {
  dpp_string_append_cstr(dst, dpp_string_c_str(&src));
  dpp_string_destroy(&src);
}

/* Return c_str of a dpp_string for use in a printf argument. NOT thread-safe.
   Makes a copy of s.data so the caller's dpp_string is not disturbed.
   The returned pointer is valid until the next call to dpp_string_tmp_cstr. */
static inline const char *dpp_string_tmp_cstr(dpp_string s) {
  static char *_dpp_tmp_buf = NULL;
  if (_dpp_tmp_buf) free(_dpp_tmp_buf);
  _dpp_tmp_buf = s.data ? strdup(s.data) : strdup("");
  return _dpp_tmp_buf;
}

/* dpp_vector-of-string helpers — declared here to avoid circular includes */
struct dpp_vector;
void dpp_vector_destroy_strings(struct dpp_vector *v);
void dpp_string_vector_push(struct dpp_vector *v, const dpp_string *s);
void dpp_string_vector_push_cstr(struct dpp_vector *v, const char *s);
