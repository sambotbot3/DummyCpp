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

Compile and run the generated C with the injected runtime:

```bash
cc build/point.c -I inject/c build/inject/libdpp_inject.a -o build/point
./build/point
echo $?
```

The example should exit with status `3`.

Or run the full example pipeline:

```bash
scripts/run_point_example.sh
```

Try newer examples by transpiling them to `build/` and compiling the generated C with the injected runtime:

```bash
build/dpp examples/class_counter.cpp -o build/class_counter.c
cc build/class_counter.c -I inject/c build/inject/libdpp_inject.a -o build/class_counter
./build/class_counter

build/dpp examples/vector_int.cpp -o build/vector_int.c
cc build/vector_int.c -I inject/c build/inject/libdpp_inject.a -o build/vector_int
./build/vector_int

build/dpp examples/multi_feature_scores.cpp -o build/multi_feature_scores.c
cc build/multi_feature_scores.c -I inject/c build/inject/libdpp_inject.a -o build/multi_feature_scores
./build/multi_feature_scores
```

Run the parity test harness:

```bash
scripts/test_all.sh
```

The harness compiles each original C++11 case, runs it, transpiles it to C, compiles the generated C, runs that binary, then compares stdout and exit status.
It also runs `tests/unsupported/` cases and expects Dpp to reject them with a syntax/support diagnostic.

## Current Bootstrap Support

This first slice is intentionally tiny and friendly-input only:

- simple `struct` declarations with fields,
- simple `class` declarations with fields,
- one-line methods lowered to `Type_method(Type *self, ...)`,
- simple constructor initializer lists lowered to `Type_init`,
- single public inheritance lowered by embedding the base record as `base`,
- free functions,
- `int main()` lowered to `int main(void)`,
- aggregate initialization like `Point p{1, 2};`.
- narrow `std::cout` chains with string literals, integer expressions, and `std::endl`.
- `std::vector<int>`, `std::vector<double>`, and `std::vector<SimpleRecord>` with `push_back`, `size`, and `[]`.
- method calls on vector-held class/record elements for the current simple lowering shape.
- loop and early-return cases around vector-backed locals, with generated cleanup before returns.
- `using namespace std;` for `cout`, `endl`, and `vector<T>`.
- early unsupported-feature diagnostics for unsupported headers, exceptions, multiple/private/protected/virtual inheritance, virtual methods, and user-defined templates.

The project direction is Clang-based, but this initial bootstrap transpiler is a narrow implementation that lets us verify the first C++ -> C -> executable loop before adding the Clang AST frontend.

See `docs/` for the plan, roadmap, decisions, resources, and condensed questions.

Project layout is described in [docs/architecture.md](docs/architecture.md).
