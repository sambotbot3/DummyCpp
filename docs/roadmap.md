# DummyCpp Roadmap

## Done In Bootstrap

- Skeleton CLI and CMake build.
- Parity harness in `scripts/test_all.sh`.
- `cout` lowering.
- Structs/classes, fields, methods, constructors.
- Single public inheritance.
- `std::vector` runtime and generated-C runtime linking.
- Condensed long workflow tests.

## Next

- Parser/keyword unit tests.
- Token-aware `cout` parsing.
- Dpp IR for supported records/functions/expressions.
- Clang AST bridge that extracts supported constructs into Dpp IR.
- Better diagnostics for unsupported constructs.
- Inheritance expansion: base views, derived-to-base passing, inherited calls inside derived methods.
- Vector expansion: direct function-return pushes, more element types, clearer cleanup model.

## Later

- Namespaces and stronger C symbol mangling.
- Overload handling.
- References and const semantics.
- Simple function templates with visible concrete instantiations.
- Multi-file input/build story.
