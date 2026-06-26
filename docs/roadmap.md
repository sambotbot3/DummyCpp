# DummyCpp Roadmap

## Done In Bootstrap

- Skeleton CLI and CMake build.
- Parity harness in `scripts/test_all.sh`.
- `cout` / `endl` lowering to `printf`.
- Structs/classes, fields, methods, constructors.
- Single and multiple non-virtual inheritance with ignored access modifiers.
- `std::vector<int/double/record>` runtime and generated-C runtime linking.
- `std::string` — basic construction, assignment, `size()`, `empty()`, `c_str()`, comparisons, `+=`.
- `std::map` / `std::unordered_map` — integral and string keys, `operator[]`, `size()`, range-for.
- `std::unique_ptr` / `std::shared_ptr` — local new, `->`, `get()`, cleanup before return.
- Simple function templates monomorphized to concrete instantiations.
- Selected `<algorithm>` functions: `sort`, `reverse`, `fill`, `min`, `max`, `swap`.
- Quoted local header expansion with `#pragma once` / include-guard caching.
- Multifile parity example in `examples/multifile_supported/`.
- Parser / keyword unit tests via CTest.

## Next

- **`-I` search paths**: add `-I <dir>` CLI flag and thread search-path list
  through the preprocessor. Required before multi-directory test cases work.
- **Name mangling**: implement overload-hash mangling from `docs/CONVENTIONS.md`
  for functions that share a name within a translation unit.
- **Dpp IR**: define a small in-memory IR for records, functions, and supported
  expression nodes. No Clang dependency yet — extract from the token stream.
- **Scope-exit cleanup**: extend lifetime tracking beyond return paths to cover
  early `break`, nested blocks, and arbitrary scope exit for runtime-backed locals.
- **`cout` type tracking**: replace the regex-based `cout` chain parser with a
  token-aware pass that can correctly handle more expression forms.
- **Inheritance expansion**: derived-to-base value passing, pointer/reference base
  views, inherited method calls inside derived method bodies.

## Later

- **Clang AST bridge**: extract supported constructs from the Clang AST into Dpp IR
  instead of parsing source text. This is the path to reliable type tracking and
  correct overload resolution.
- **Per-header `.dh` transpilation**: transpile user headers once to `.dh` files in
  the build directory; each TU `#include`s the shared `.dh` instead of re-expanding
  the header inline. See `docs/architecture.md`.
- **On-disk header cache**: cache transpiled header IR by canonical path + content
  hash to speed up incremental multi-file builds.
- **Full multi-file build**: `dpp` accepts multiple `.cpp` files, emits a `.c` per
  source and a `.dh` per header, and optionally emits a build script.
- **Namespace lowering**: implement `namespace_name_` prefix mangling for nested
  namespaces; resolve `using namespace` in lowering passes.
- **Overload resolution**: basic overload disambiguation using the mangled names.
- **References and const semantics**: lower `const T &` parameters to `const T *`.
- **Virtual dispatch**: vtable structs with function pointer fields; `dynamic_cast`
  as a known-unsupported diagnostic.
- **`string_view` and `span`**: narrow read-only slice types as pointer + length structs.
