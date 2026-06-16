# Injected C Runtime Segments

This directory is for reusable C code that Dpp can inject or emit alongside generated C when a lowered C++ feature needs helper runtime support.

Examples:

- `c/dpp_vector.h` and `c/dpp_vector.c` for a future `std::vector` lowering.
- string helpers for a future `std::string` lowering.
- allocation/error helpers when a feature cannot remain zero-overhead.

Keep injected code:

- plain C,
- small,
- feature-scoped,
- documented with the C++ feature that needs it.

