# Open Source Resources

Use these as references, not all as dependencies.

## Compiler Frontends / AST

- LLVM / Clang
  - Parser, AST, diagnostics, libTooling examples, C++ language behavior.
  - Useful for first implementation if we choose an AST-driven lowerer.
- libclang
  - C API for Clang AST traversal.
  - Useful if we want a stable-ish API boundary.
- tree-sitter-cpp
  - Incremental syntax parser for C++.
  - Good for lightweight parsing experiments, not full semantic analysis.

## C++ Implementations / Standard Library References

- libc++
  - LLVM C++ standard library implementation.
- libstdc++
  - GCC C++ standard library implementation.
- Microsoft STL
  - Open source MSVC standard library implementation.
- cppreference
  - Behavioral reference for standard-library features and language details.

## C / ABI / Runtime References

- Itanium C++ ABI
  - Useful later for understanding name mangling, vtables, object layout.
- C11/C17 references
  - Generated C target constraints.
- musl libc
  - Small, readable libc implementation patterns.

## Related / Inspirational Projects

- Cfront history
  - Original C++ to C translator concept.
- Clang AST matchers examples
  - Good starting point for feature-specific lowering.
- Emscripten / Binaryen docs
  - Useful for thinking about lowering pipelines, even though target is different.
- Cython / Nim / Vala generated C
  - Useful examples of readable generated C plus runtime helpers.

## Test Sources

- Tiny hand-written examples first.
- Compiler explorer examples for C++ constructs.
- libc++ / libstdc++ tests later, filtered to supported features.
- Small single-header libraries once methods/constructors/templates exist.

## License Notes

- Keep copied code out of Dpp unless license compatibility is explicit.
- Prefer references and clean-room reimplementation.
- If importing tests or snippets, record source and license near the imported file.

