# DummyCpp Roadmap

Current detailed next-step plan: `docs/next-steps-parser-conversion-plan.md`.

## Milestone 1 - Skeleton CLI

- CMake project.
- `dpp --help`.
- Accept input/output paths.
- Copy-through or stub emit for a trivial `int main() { return 0; }` program.
- Compile generated C with `cc`.
- Treat generated-C compilation as the minimum verification gate.
- Add `scripts/test_all.sh` for C++ vs generated-C stdout/status parity.
- Add `std::cout`/`std::endl` lowering early so tests can assert printed behavior.

## Milestone 2 - Structs And Functions

- Parse/lower simple structs.
- Parse/lower free functions.
- Lower aggregate initialization.
- Add `examples/point.cpp`.
- Add regression test that generated C compiles and exits correctly.
- Keep examples small.

## Milestone 3 - Methods

- Lower simple methods to free functions.
- Lower member access in methods from implicit `this` to explicit `self`.
- Add deterministic C name generation.
- Add `tests/cases/005_struct_method.cpp`.

## Milestone 4 - Constructors

- Lower simple constructors to init functions.
- Support local object initialization.
- Add explicit lifetime notes.
- Add `tests/cases/004_class_counter.cpp`.

## Milestone 5 - Vector Runtime

- Finish `inject/c/dpp_vector.h` and `inject/c/dpp_vector.c`.
- Lower `std::vector<int>` to `dpp_vector`.
- Compile generated C with required runtime sources.
- Add `tests/cases/006_vector_int.cpp`.
- Then add vector of simple POD records with `tests/cases/007_vector_record.cpp`.

## Milestone 6 - Templates Lite

- Support explicit visible template instantiations.
- Generate one C symbol per instantiation.
- Start with function templates, then class templates.

## Milestone 7 - Usability

- Better diagnostics.
- Multi-file input story.
- Optional CMake output generation.
- Feature coverage table.
