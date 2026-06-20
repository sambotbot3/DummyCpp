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
  assignment from literals or supported strings, `size()`, and `c_str()`.
- Unsupported: concatenation, comparison operators, mutation APIs, substrings,
  find/search APIs, iterators, and string fields inside records/classes.
- Limitation: cleanup is generated for return paths in the current bootstrap
  style; arbitrary scope-exit cleanup is still incomplete.

## `<vector>`

- Supported: local vectors with `push_back`, initializer lists, `size()`,
  `empty()`, `clear()`, `back()`, and `[]` for the current supported element
  types.
- Unsupported: `reserve`, `resize`, `pop_back`, `front`, iterators outside
  supported `<algorithm>` full-range calls, and nested vectors.

## Preprocessor

- Supported: quoted local includes are expanded before syntax checking.
- Supported: `#pragma once` and simple `#ifndef`/`#define` include guards avoid
  duplicate guarded header injection.
- Supported: angle includes for Dpp-supported C++ standard headers are preserved
  for lowering passes rather than expanded.
- Unsupported: macros, conditional compilation, full include search paths, and
  system-header expansion.
