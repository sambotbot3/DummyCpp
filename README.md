# DummyCpp (Dpp)

DummyCpp, or Dpp, is a small C++ to C transpiler project.

The goal is not to support all of C++ immediately. The goal is to make a useful, expanding C++11 subset lower to compact, inspectable C, compile with a normal C compiler, and grow feature coverage over time.

## Quickstart

Install dependencies on Ubuntu/Debian:

```bash
scripts/install_dependencies_ubuntu.sh
```

Build `dpp`:

```bash
cmake -S . -B build
cmake --build build
```

Transpile the starter example:

```bash
build/dpp examples/point.cpp -o build/point.c
```

Compile and run the generated C:

```bash
cc build/point.c -o build/point
./build/point
echo $?
```

The example should exit with status `3`.

Or run the full example pipeline:

```bash
scripts/run_point_example.sh
```

Run the parity test harness:

```bash
scripts/test_all.sh
```

The harness compiles each original C++11 case, runs it, transpiles it to C, compiles the generated C, runs that binary, then compares stdout and exit status.

## Current Bootstrap Support

This first slice is intentionally tiny and friendly-input only:

- simple `struct` declarations with fields,
- free functions,
- `int main()` lowered to `int main(void)`,
- aggregate initialization like `Point p{1, 2};`.
- narrow `std::cout` chains with string literals, integer expressions, and `std::endl`.

The project direction is Clang-based, but this initial bootstrap transpiler is a narrow implementation that lets us verify the first C++ -> C -> executable loop before adding the Clang AST frontend.

See `docs/` for the plan, roadmap, decisions, resources, and condensed questions.

Project layout is described in [docs/architecture.md](docs/architecture.md).
