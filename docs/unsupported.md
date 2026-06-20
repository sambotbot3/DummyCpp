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
- Unsupported: most of `<algorithm>`, including search, count, copy, transform,
  remove, partition, heap, merge, stable sort, and binary-search algorithms.
- Limitation: `DPP_SORT_VECTOR` is currently insertion sort, so generated C is
  visible and helper-free but does not match `std::sort` complexity.
- Limitation: macro lowerings can evaluate arguments more than once for
  `DPP_MIN` and `DPP_MAX`; avoid side-effecting arguments until these lowerings
  become expression-safe helpers.

Regression coverage lives in `tests/cases/020_algorithm_sort.cpp` and
`tests/cases/021_algorithm_more.cpp`.
