# DummyCpp (Dpp)

DummyCpp, or Dpp, is a small C++ to C transpiler project.

The goal is not to support all of C++ immediately. The goal is to make a useful, expanding C++11 subset lower to compact, inspectable C, compile with a normal C compiler, and grow feature coverage over time.

## Quickstart

On Ubuntu/Debian, install dependencies with `scripts/install_dependencies_ubuntu.sh`.

Build `dpp` with CMake, or use `scripts/build.sh` to regenerate the lexer and build in one step.

The lexer is maintained in `src/parser/lexer.re` for re2c `4.5.1`. After editing `.re`
lexer sources, regenerate the checked-in C++ source with `scripts/gen_lexer.sh`.

Try the starter pipeline with `scripts/run_point_example.sh`. It builds the tool,
transpiles `examples/point.cpp`, compiles the generated C with the injected runtime,
and expects the resulting program to exit with status `3`.

Run the full parity harness with `scripts/test_all.sh`.

The harness compiles each original C++11 case, runs it, transpiles it to C,
compiles the generated C, runs that binary, then compares stdout and exit status.
It also runs `tests/unsupported/` cases and expects Dpp to reject them with a
syntax/support diagnostic.

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
- `std::unique_ptr<T>` and `std::shared_ptr<T>` for local `new T(...)` ownership, simple `->`/`get()` access, and shared-pointer copies.
- early unsupported-feature diagnostics for unsupported headers, exceptions, multiple/private/protected/virtual inheritance, virtual methods, and user-defined templates.

The project direction is Clang-based, but this initial bootstrap transpiler is a narrow implementation that lets us verify the first C++ -> C -> executable loop before adding the Clang AST frontend.

See `docs/` for the plan, roadmap, decisions, resources, and condensed questions.

Project layout is described in [docs/architecture.md](docs/architecture.md).
