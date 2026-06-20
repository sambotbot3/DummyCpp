# Unsupported Bootstrap Portions

This document tracks known unsupported or intentionally partial areas in the
current bootstrap transpiler. Keep entries concise and move items out when parity
tests prove support.

## `<algorithm>`

Current support is intentionally narrow:

- Supported: `std::sort`, `std::reverse`, and `std::fill` only for full
  `std::vector<T>` ranges written as `xs.begin(), xs.end()`.
- Supported: `std::min`, `std::max`, and `std::swap` as macro lowerings for
  simple scalar expressions/lvalues.
- Unsupported: iterator subranges such as `xs.begin() + 1` or `xs.end() - 1`.
- Unsupported: comparator overloads, projection-like patterns, and custom
  ordering callbacks.
- Unsupported: non-vector iterators, raw pointer ranges, arrays, maps, and
  initializer-list algorithm inputs.
- Unsupported: sorting record/class vectors until the converter can find or
  generate a matching comparator function.
- Limitation: the multifile example currently accepts and skips a lambda
  comparator sort only for a narrow record-vector case where parity does not
  depend on reordering.
- Unsupported: most of `<algorithm>`, including search, count, copy, transform,
  remove, partition, heap, merge, stable sort, and binary-search algorithms.
- Limitation: `DPP_SORT_VECTOR` lowers supported scalar vectors to C `qsort`,
  which is explicit but not stable.
- Limitation: macro lowerings can evaluate arguments more than once for
  `DPP_MIN` and `DPP_MAX`; avoid side-effecting arguments until these lowerings
  become expression-safe helpers.

Regression coverage lives in `tests/cases/020_algorithm_sort.cpp` and
`tests/cases/021_algorithm_more.cpp`.

## `<string>`

- Supported: local `std::string` variables with literal/copy construction,
  assignment from literals or supported strings, `size()`, `empty()`, `c_str()`,
  `clear()`, `+=`, and statement-form `append()`.
- Supported: narrow `==`, `!=`, and ordering comparisons between supported
  local strings and string literals.
- Unsupported: concatenation, broader mutation APIs, substrings, find/search
  APIs, iterators, and full record/class field lifecycle support.
- Supported: simple record aggregate locals with `std::string` fields initialize
  those fields with explicit `dpp_string_init_cstr` calls.
- Limitation: full constructors/copy/destruction for record/class string fields
  are still incomplete.
- Limitation: cleanup is generated for return paths in the current bootstrap
  style; arbitrary scope-exit cleanup is still incomplete.

## `<vector>`

- Supported: local vectors with `push_back`, initializer lists, `size()`,
  `empty()`, `clear()`, `reserve()`, `resize()`, `front()`, `back()`,
  `pop_back()`, fill-form `resize(count, value)`, and `[]` for the current
  supported element types.
- Supported: narrow const vector copy declarations and range-for iteration for
  record vectors used by the multifile example.
- Unsupported: iterators outside supported `<algorithm>` full-range calls and
  nested vectors.

## `<map>` and `<unordered_map>`

- Supported: local maps with integral keys and values, `operator[]`, and
  `size()` in narrow single-file cases.
- Supported: local `std::map<std::string, int>` string-key maps for literal keys.
- Supported: narrow `std::map<std::string, T>` typedef aliases, alias locals,
  const-reference parameters, `operator[]`, `size()`, and range-for iteration
  used by the multifile example.
- Unsupported: general iterators, erase/find APIs, custom comparators/hashers,
  non-string record keys, and broad cross-file map API shapes.

## Templates

- Supported: simple free-function templates with `typename`/`class` type
  parameters, explicit calls, narrow single-argument scalar inference, and
  simple record-variable inference.
- Unsupported: class templates beyond recognized standard library spellings,
  non-type parameters, specialization, overload resolution, SFINAE, variadic
  templates, and template bodies that need unsupported parameter/container
  lowering.
- Known-fail coverage lives in `examples/multifile_known_fail/` and can be run
  with `scripts/run_multifile_known_fail_example.sh`.

## Preprocessor

- Supported: quoted local includes are expanded before syntax checking.
- Supported: `#pragma once` and simple `#ifndef`/`#define` include guards avoid
  duplicate guarded header injection.
- Supported: angle includes for Dpp-supported C++ standard headers are preserved
  for lowering passes rather than expanded.
- Unsupported: macros, conditional compilation, full include search paths, and
  system-header expansion.
