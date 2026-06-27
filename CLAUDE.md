# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Never put an author in a commit message. And keep them brief, like a subject line on an email.
Never do a git push to master/main.

## Project Overview

DummyCpp (`dpp`) is a C++11-to-C transpiler written in C++. It lowers a narrow C++ subset to readable, inspectable C, compiles that C with a plain C compiler, and verifies correctness by diffing stdout/exit-status between the C++ and C builds.

Difficult to implement and barely used portions of c++ will be noted in docs/ and ignored. This includes things like exceptions. 

The point of this is to avoid the C++ runtime. This transpiler now has to handle alot of the complexity that the virtual tables handles in C++. 

The transpiled c should be as readable as possible, however sometimes messy macros may be best. 

## Build Commands

```bash
# Install Ubuntu/Debian dependencies
scripts/install_dependencies_ubuntu.sh

# Regenerate lexer then build (standard dev workflow)
scripts/build.sh

# Build only (CMake)
cmake -S . -B build && cmake --build build

# Run the full parity + unsupported test harness
scripts/test_all.sh

# Run CTest unit tests (parser tests only)
ctest --test-dir build --output-on-failure

# Regenerate lexer after editing src/parser/lexer.re
scripts/gen_lexer.sh
```

The lexer requires **re2c 4.5.1**. `scripts/gen_lexer.sh` warns on version mismatch.

Running a single parity test manually:
```bash
build/dpp tests/cases/001_cout.cpp -o /tmp/out.c
cc /tmp/out.c -I inject/c build/inject/libdpp_inject.a -lm -o /tmp/out && /tmp/out
```

## Architecture

The transpiler is a sequential pipeline of text-rewriting passes, not an AST-based system. All code is in the `dpp` namespace.

### Libraries

| Target | Location | Role |
|---|---|---|
| `dpp_utils` | `src/string_utils.cpp` | Shared string helpers (public API: `include/dpp/string_utils.h`) |
| `dpp_parser` | `src/parser/` | Tokenize, keyword-classify, parse function templates, syntax/support diagnostics |
| `dpp_convert` | `src/convert/` | Feature-scoped lowering passes (one `.cpp` + header per feature) |
| `dpp_transpiler` | `src/transpiler.cpp` | Orchestrates the full pipeline; CLI thin wrapper calls this |
| `dpp_inject` | `inject/` | Plain-C runtime for generated executables (`dpp_vector`, `dpp_string`, `dpp_map`, `dpp_memory`, `dpp_algorithm`) |

### Pipeline order (`src/transpiler.cpp`)

```
parse_translation_unit → check_bootstrap_syntax
→ lower_function_templates → lower_records → lower_vectors → lower_maps
→ lower_algorithms → lower_memory → lower_strings → lower_iostreams
→ lower_assertions → lower_structs → lower_main_signature
→ lower_aggregate_initializers → lower_cpp_surface_types
→ prepend required #includes
```

Each pass returns a result struct that carries the rewritten source text and a boolean flag (`used_vector`, `used_string`, etc.); the transpiler uses those flags to decide which `inject/c/` headers to emit at the top of the generated file.

### Parser

- `src/parser/lexer.re` — re2c source; **never hand-edit `lexer.cpp`**, always regenerate via `scripts/gen_lexer.sh`.
- `src/parser/parser.cpp` — recursive descent for declarations, Pratt parsing for expressions. Produces `ParsedSource` (token stream + function template decls).
- `src/parser/syntax_checker.cpp` — early rejection of unsupported features (exceptions, virtual, unsupported headers, etc.) before conversion runs.
- `src/parser/preprocessor.cpp` — quoted local header expansion with `#pragma once` / include-guard caching.

### Injected C Runtime (`inject/c/`)

Runtime symbols use the `dpp_` prefix. Each `.h`/`.c` pair maps to one C++ standard library feature:
- `dpp_vector` — `std::vector<int/double/record>`
- `dpp_string` — `std::string`
- `dpp_map` — `std::map` / `std::unordered_map`
- `dpp_memory` — `std::unique_ptr` / `std::shared_ptr`
- `dpp_algorithm` — selected `<algorithm>` functions

When adding a new runtime feature, update `inject/CMakeLists.txt` and `inject/README.md`.

## Testing

- **Parity tests** (`tests/cases/`) — Each `.cpp` file is compiled with `c++` and with `dpp`+`cc`; stdout and exit status must match. Run via `scripts/test_all.sh`.
- **Unsupported tests** (`tests/unsupported/`) — `dpp` must exit non-zero with a `syntax check failed`, `preprocess failed`, or `template instantiation failed` diagnostic.
- **Parser unit tests** (`tests/parser_tests.cpp`) — linked against `dpp_parser`, run via CTest.
- **Multifile tests** (`examples/multifile_supported/`, `examples/multifile_known_fail/`) — run via `scripts/run_multifile_supported_example.sh`.

After adding a supported feature, add or update a case in `tests/cases/` and verify with `scripts/test_all.sh`.

## Conventions

- Use `trash` instead of `rm`.
- Common string utilities belong in `include/dpp/string_utils.h` / `src/string_utils.cpp`.
- Document unsupported/skipped features in `docs/unsupported.md`; write a `TODO` comment at the skip site.
- Open questions go in `docs/questions.md`; once resolved, move to `docs/decisions.md`.
- Larger project notes belong in `docs/`.
- Multifile example programs belong in `examples/`.
- Public APIs live under the `dpp` namespace; module-internal APIs use nested namespaces (`dpp::convert`, `dpp::parser`); implementation helpers use anonymous namespaces in `.cpp` files.
- Each conversion pass has a matching header in `include/dpp/convert/`; update `CMakeLists.txt` when adding source files.
- Preserve pass ordering in `src/transpiler.cpp` when adding inter-pass dependencies.
- Anytime we're discussing overarching themes for the transpiler, read and update docs/CONVENTIONS.md.
