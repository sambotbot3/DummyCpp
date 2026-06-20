#pragma once

#include <stddef.h>

typedef struct dpp_string {
  char *data;
  size_t size;
} dpp_string;

void dpp_string_init(dpp_string *string);
void dpp_string_init_cstr(dpp_string *string, const char *value);
void dpp_string_assign_cstr(dpp_string *string, const char *value);
void dpp_string_destroy(dpp_string *string);
size_t dpp_string_size(const dpp_string *string);
const char *dpp_string_c_str(const dpp_string *string);
