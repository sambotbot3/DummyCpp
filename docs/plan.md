# DummyCpp Plan

## Goal

DummyCpp (`dpp`) is a small C++11-to-C transpiler. It should emit compact, inspectable C and prove each supported feature by compiling and running generated C.

The project is intentionally not trying to be a full C++ compiler yet. It should grow by small, verified slices.

## Working Strategy

- Use C++ for the transpiler.
- Keep the CLI thin; implementation belongs in libraries.
- Keep generated-C compilation as the minimum acceptance gate.
- Prefer behavioral parity tests: original C++ stdout/status must match generated C stdout/status.
- Keep newer tests condensed into broader workflow files unless a feature needs a focused bootstrap case.
- Use Clang/LLVM as the eventual parsing source of truth, but keep the bootstrap pipeline useful while that bridge is built.

## Implemented Bootstrap Coverage

- CLI build with CMake.
- `std::cout` / `std::endl` to `printf`.
- Free functions and primitive expressions.
- Struct/class fields.
- One-line methods lowered to explicit `self` functions.
- Constructors lowered to `Type_init`.
- Aggregate initialization for simple records.
- Single and multiple non-virtual inheritance as embedded base fields, with access modifiers ignored.
- `std::vector` runtime for `int`, `double`, and simple record/class values.
- Narrow `<algorithm>` support for selected vector-range algorithms and scalar
  macros; unsupported portions are tracked in `docs/unsupported.md`.
- Runtime linking in `scripts/test_all.sh`.
- Expected-failure tests for unsupported features.

## Next Engineering Direction

- Replace more regex/string conversion with token-aware or IR-backed conversion.
- Add parser/keyword tests.
- Build a tiny Dpp IR before going deeper on Clang AST extraction.
- Improve diagnostics and unsupported-feature reporting.
- Expand inheritance and vector support only through green parity tests.

See `docs/next-steps-parser-conversion-plan.md` for the active short checklist.
