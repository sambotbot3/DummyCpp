# Injected C Runtime

This directory contains reusable C code that Dpp links with generated C when a
lowered C++ feature needs runtime support. The goal is to keep direct C ports of
C++ classes and standard-library-like pieces in one place so generated programs
can reuse them instead of emitting duplicate helper code.

Current support:

- `c/dpp_vector.h` and `c/dpp_vector.c` provide the reusable backing store for
  supported `std::vector<T>` lowerings.
- `c/dpp_map.h` and `c/dpp_map.c` provide explicit runtime backing for the
  supported `std::map<K, int>` and `std::unordered_map<K, int>` lowerings where
  `K` is `char`, `int`, or `long`.
- `c/dpp_string.h` and `c/dpp_string.c` provide explicit runtime backing for
  selected `std::string` local variables and `c_str()`/`size()` calls.
- `c/dpp_algorithm.h` provides header-only macro lowerings for selected
  `<algorithm>` calls: `std::sort`, `std::reverse`, and `std::fill` over
  supported `std::vector<T>` ranges, plus `std::min`, `std::max`, and
  `std::swap`.

Future support may include allocation/error helpers when a feature cannot remain
zero-overhead.

Keep injected code:

- plain C,
- small,
- feature-scoped,
- documented with the C++ feature that needs it,
- reusable enough that every generated-C example can link it unconditionally.
