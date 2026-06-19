# AGENTS.md - Conversion Passes

Scope: C++-subset to C lowering passes under `src/convert/`.

- Keep each pass feature-scoped, with matching declarations in `include/dpp/convert/`.
- Return metadata such as required runtime includes through small result structs.
- Prefer narrow transformations that support real tests over broad partial C++ parsing.
- Preserve pass ordering expectations in `src/transpiler.cpp` when adding dependencies between passes.
- When adding supported syntax, add or update parity tests and an example if it is user-facing.

