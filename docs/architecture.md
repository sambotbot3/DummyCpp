# DummyCpp Architecture

## Design Principles

- The transpiler is a **thin CLI** (`dpp`) over a set of focused libraries.
  Implementation code belongs in libraries, not in `main`.
- The pipeline is **text-rewriting** today. A structured IR and Clang AST bridge
  are planned next steps; the text pipeline stays green in the meantime.
- Generated C must be **readable and inspectable**. A developer should be able to
  read the `.c` output and understand what the original C++ was doing.
- All lowering rules live in `docs/CONVENTIONS.md`. When a pass makes a lowering
  decision, it must follow those rules.

---

## Libraries

| CMake target | Location | Role |
|---|---|---|
| `dpp_utils` | `src/string_utils.cpp` | Shared string helpers; public API in `include/dpp/string_utils.h` |
| `dpp_parser` | `src/parser/` | Lex, classify keywords, parse templates, preprocess includes, syntax/support diagnostics |
| `dpp_convert` | `src/convert/` | All C++-to-C lowering passes; one `.cpp` + matching `include/dpp/convert/*.h` per feature |
| `dpp_transpiler` | `src/transpiler.cpp` | Orchestrates the full pipeline in the correct pass order |
| `dpp_inject` | `inject/c/` | Plain-C runtime for generated executables (`dpp_vector`, `dpp_string`, `dpp_map`, `dpp_memory`, `dpp_algorithm`) |

The CLI (`src/main.cpp`) only reads files, calls `preprocess_translation_unit_file`
then `transpile_bootstrap_subset`, and writes the output. No lowering logic lives there.

---

## Full Pipeline

```
CLI reads .cpp file
  │
  ▼
preprocess_translation_unit_file   (dpp_parser / preprocessor.cpp)
  • Expand quoted local headers inline (recursive, cycle-detected)
  • Strip #pragma once / include-guard lines to avoid re-injection
  • Pass angle-bracket includes through unchanged for lowering passes
  • Cache: per-invocation in-memory cache keyed by canonical path
  │
  ▼
parse_translation_unit             (dpp_parser / parser.cpp)
  • Tokenize with re2c lexer
  • Classify keywords
  • Parse function template declarations
  │
  ▼
check_bootstrap_syntax             (dpp_parser / syntax_checker.cpp)
  • Reject unsupported headers, exceptions, virtual, unsupported templates
  • Error → abort with diagnostic; warning → continue
  │
  ▼
Conversion passes                  (dpp_convert, in order)
  lower_function_templates         expand template<T> to concrete instantiations
  lower_records                    struct/class → C struct + method free-functions
  lower_vectors                    std::vector<T> → dpp_vector_T calls
  lower_maps                       std::map / std::unordered_map → dpp_map calls
  lower_algorithms                 selected <algorithm> → dpp_algorithm macros/calls
  lower_memory                     unique_ptr / shared_ptr → dpp_memory helpers
  lower_strings                    std::string → dpp_string calls
  lower_iostreams                  std::cout chains → printf
  lower_assertions                 assert() → C assert()
  lower_structs                    remaining struct/class syntax cleanup
  lower_main_signature             int main() → int main(void)
  lower_aggregate_initializers     Type t{...} → C struct literals / init calls
  lower_cpp_surface_types          strip remaining C++ surface syntax
  │
  ▼
Emit header block                  (#include directives for used inject headers)
  │
  ▼
Write .c output file
```

Each conversion pass returns a result struct carrying the rewritten source text and
boolean flags (`used_vector`, `used_string`, etc.). The transpiler collects these
flags and prepends only the `#include "dpp_*.h"` headers that are actually needed.

---

## Parser / Frontend

### Lexer

- Source: `src/parser/lexer.re` (re2c 4.5.1 input).
- Generated output: `src/parser/lexer.cpp` — **never hand-edit this file**.
- Regenerate with `scripts/gen_lexer.sh` after any `.re` edit.
- The lexer produces a flat token stream; keyword classification happens in `parser.cpp`.

### Parser

`src/parser/parser.cpp` is a two-phase handwritten parser:
- **Tokenization**: delegates to the re2c lexer, then classifies keyword tokens.
- **Template extraction**: a recursive-descent pass over the token stream that finds
  `template<...>` declarations and extracts `FunctionTemplateDecl` records.

The `ParsedSource` type carries the full source text, the flat token list, and the
function template list. Downstream conversion passes mostly operate on the source
text; they use the token list and template list only where text matching is too fragile.

### Preprocessor

`src/parser/preprocessor.cpp` handles quoted local header expansion **before**
tokenization. Key behaviors:

- **Search strategy**: resolves relative to the including file only. No `-I` search
  paths yet — see *Planned: Header Search Paths* below.
- **Cycle detection**: maintains an active-stack and aborts on re-entry.
- **`#pragma once` / include guards**: detected and stripped; re-included guarded
  headers produce empty output instead of a duplicate expansion.
- **In-memory cache**: `Context::cache` stores the expanded text of each canonical
  path for the duration of one `dpp` invocation, so headers included from multiple
  places are only read and expanded once.
- **Angle includes**: left in-place for conversion passes; the preprocessor does not
  try to find system headers on disk.
- **Security**: rejects absolute paths, NUL bytes in paths, and paths that escape
  the root directory of the input file.

### Syntax Checker

`src/parser/syntax_checker.cpp` runs after preprocessing and tokenization. It
rejects unsupported features (exceptions, virtual, unsupported stdlib headers, class
templates) and emits warnings for things like using `std::cout` without the right
include. Hard errors abort the pipeline; warnings do not.

---

## Injected C Runtime (`inject/c/`)

Each `.h`/`.c` pair provides a plain-C port of one C++ standard library feature:

| Files | C++ feature |
|---|---|
| `dpp_vector.h/.c` | `std::vector<int>`, `std::vector<double>`, vector of POD records |
| `dpp_string.h/.c` | `std::string` |
| `dpp_map.h/.c` | `std::map`, `std::unordered_map` |
| `dpp_memory.h/.c` | `std::unique_ptr`, `std::shared_ptr` |
| `dpp_algorithm.h` | Selected `<algorithm>` functions (header-only macros/statics) |

All symbols use the `dpp_` prefix. The runtime is built as `libdpp_inject.a` and
linked into every generated-C executable.

When adding a new runtime module: add files to `inject/c/`, update
`inject/CMakeLists.txt`, and update `inject/README.md`.

---

## Header Handling — Current vs. Target

### Current (single-file inline expansion)

All included headers are inlined into the source as a preprocessing step. The
preprocessor produces one expanded source string, which the conversion pipeline
processes as a unit. The in-memory `Context::cache` prevents headers from being
re-read or re-expanded more than once per `dpp` invocation.

**Pros**: simple, no build-directory bookkeeping, works for single-file programs.  
**Cons**: headers are re-processed for every TU that includes them; type definitions
appear multiple times if multiple TUs are eventually linked; template instantiations
are duplicated.

### Planned: Header Search Paths (`-I`)

Add a `-I <dir>` flag to the CLI and thread a search-path list through the
`PreprocessContext`. Resolution order: relative to the including file first, then
each `-I` directory in declaration order. This mirrors GCC's quoted-include
behavior and is required before multi-file builds can work.

### Planned: Per-Header Transpiled Output (`.dh` files)

For multi-file projects the correct model is:

1. Each user header (`.h`) is transpiled once to a corresponding **DPP header**
   (`.dh`) written to the build directory. The `.dh` file contains C struct and
   function declarations that all TUs sharing the header will agree on.
2. Each `.cpp` is transpiled to a `.c` file. Its `#include "foo.h"` directives
   become `#include "foo.dh"` (resolved in the build directory).
3. A function template defined in a user header emits a `static` concrete function
   per TU that instantiates it (monomorphize-per-TU; the linker discards duplicates).
4. An on-disk cache in `build/dpp_cache/` (keyed by canonical path + content hash)
   avoids re-transpiling unchanged headers across incremental builds.

This keeps the compilation model close to what C already does: each TU is
independent, headers are shared declarations, and the linker combines object files.

### Planned: Build Directory Layout

```
build/
  foo.c                        transpiled source for foo.cpp
  dpp_headers/
    include/bar.dh             transpiled C declarations for include/bar.h
  dpp_cache/
    <sha256>.ir                cached DPP IR blobs for unchanged headers
  inject/
    libdpp_inject.a
```

---

## Multi-File Build Story

Today multi-file support works by inlining all headers into each `.cpp` and relying
on the fact that identical struct definitions in multiple TUs are layout-compatible
in C. This breaks when two TUs produce different definitions for the same type (e.g.
because a header is included through different paths or with different macro state).

The path to correct multi-file support:

1. **Implement `-I` flag** so the preprocessor can find headers outside the source
   directory.
2. **Transpile headers to `.dh` files** instead of inlining. All TUs sharing the
   same header agree on one canonical set of C declarations.
3. **Implement name mangling** (see `docs/CONVENTIONS.md`) so overloaded functions
   in shared headers do not collide at link time.
4. **Emit `static` template instantiations** in each TU so cross-TU template use
   does not produce undefined-symbol errors at link time.

---

## Dpp IR (Planned)

The text-rewriting pipeline is a practical first step but has limits: passes must
infer type information from source text, which leads to fragile regex-based matching.
The planned successor is a small **Dpp IR** that sits between the parser and the
conversion passes:

- Structs/classes → `DppRecord { name, fields[], methods[], bases[] }`
- Functions → `DppFunction { name, return_type, params[], body_tokens[] }`
- Expressions → `DppExpr { kind, children[], type }` (narrow subset)
- Runtime needs → flags on IR nodes (`needs_cleanup`, `is_vector_backed`, etc.)

A Clang AST bridge would populate this IR for supported constructs. The conversion
passes would consume the IR instead of raw source text, enabling reliable type
tracking, correct name mangling, and accurate scope-exit cleanup generation.

---

## Adding a New Lowering Pass

1. Add `include/dpp/convert/<feature>.h` with the pass function declaration and
   its result struct.
2. Add `src/convert/<feature>.cpp` with the implementation.
3. Add the `.cpp` to `src/convert/CMakeLists.txt`.
4. Call the pass in `src/transpiler.cpp` at the correct pipeline position.
5. Add a `tests/cases/NNN_<feature>.cpp` parity test and run `scripts/test_all.sh`.
