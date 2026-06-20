# DummyCpp Plan

## Goal

DummyCpp (`dpp`) is a small C++11-to-C transpiler. It should emit compact, inspectable C and prove each supported feature by compiling and running generated C.

The project is intentionally not trying to be a full C++ compiler yet. It should grow by small, verified slices.
The goal is to compile, c++ as c to avoid numerous ABI/versioning issues.
We have to be creative to replace the c++ runtime as well. The parser has to be smarter and track these complex processes c++ handles during runtime. For example the parser has to keep track of unique_ptr descoping.

## Working Strategy

- Use C++ for the transpiler.
- Keep the CLI thin; implementation belongs in libraries.
- Keep generated-C compilation as the minimum acceptance gate.
- Prefer behavioral parity tests: original C++ stdout/status must match generated C stdout/status.
- Keep newer tests condensed into broader workflow files unless a feature needs a focused bootstrap case.
- Keep the bootstrap parser handwritten: recursive descent for declarations/statements and Pratt parsing for expressions.

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

See `docs/next-steps-parser-conversion-plan.md` for the active short checklist.

## Immediate Scope

- Add focused parser unit tests before broadening syntax coverage.
- Keep the text-lowering pipeline green while extracting shared helpers from duplicated pass logic.
- Improve lifetime cleanup for runtime-backed locals (`std::vector`, `std::string`, smart pointers) through small parity tests.
- Defer full IR work until parser tests and cleanup behavior are stable.
