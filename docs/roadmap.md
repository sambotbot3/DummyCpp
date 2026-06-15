# DummyCpp Roadmap

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

## Milestone 4 - Constructors

- Lower simple constructors to init functions.
- Support local object initialization.
- Add explicit lifetime notes.

## Milestone 5 - Tiny Runtime

- Add `dpp_runtime.h`.
- Add first stdlib-like helper: likely `string_view`, `array`, or `span`.
- Add examples and generated C checks.

## Milestone 6 - Templates Lite

- Support explicit visible template instantiations.
- Generate one C symbol per instantiation.
- Start with function templates, then class templates.

## Milestone 7 - Usability

- Better diagnostics.
- Multi-file input story.
- Optional CMake output generation.
- Feature coverage table.
