# DummyCpp Architecture

## Libraries

`dpp` should stay a thin CLI. Implementation code belongs in libraries.

## Parser / Syntax Library

Location:

- Headers: `include/dpp/parser/`
- Implementation: `src/parser/`
- CMake: `src/parser/CMakeLists.txt`
- Target: `dpp_parser`

Responsibilities:

- source loading model after the CLI passes text in,
- parser frontend integration,
- syntax diagnostics,
- eventually Clang AST entry points.

The current parser is only a bootstrap pass-through, but the library boundary is in place.

The next planned shape is a token layer, keyword classifier, Clang AST bridge, and small Dpp IR. See `docs/next-steps-parser-conversion-plan.md`.

## Conversion Library

Location:

- Headers: `include/dpp/convert/`
- Implementation: `src/convert/`
- CMake: `src/convert/CMakeLists.txt`
- Target: `dpp_convert`

Each conversion family should get a focused header and implementation.

Examples:

- `include/dpp/convert/iostream.h`
- `include/dpp/convert/basic.h`
- future: `include/dpp/convert/vector.h`
- future: `include/dpp/convert/string_view.h`

All public APIs use the `dpp` namespace.

## Transpiler Orchestration

Location:

- Header: `include/dpp/transpiler.h`
- Implementation: `src/transpiler.cpp`
- Target: `dpp_transpiler`

Responsibilities:

- call parser/syntax checks,
- run conversion passes in order,
- decide which C runtime snippets/includes are needed,
- produce final generated C text.

## Injected C Runtime

Location:

- `inject/`

Reusable C segments live here when generated code needs helpers.

Example future `std::vector` direction:

```c
typedef struct dpp_vector {
  void *data;
  size_t elem_size;
  size_t size;
  size_t capacity;
} dpp_vector;
```

Keep injected C plain, small, and feature-scoped.

The first runtime-backed standard-library target is planned as `std::vector<int>`, then vector of simple POD records.
