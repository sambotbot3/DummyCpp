# Condensed Questions For Sam

This is the single place to collect questions. Do not scatter new questions across random notes.

## Answered

- Naming: keep `DummyCpp`, `Dpp`, and CLI name `dpp`.
- Source target: C++11 first.
- Generated C style: readable, but compactness matters more when there is a tradeoff.
- Frontend: Clang-based is acceptable for now.
- First examples: small examples.
- Standard library namespace: support selected `std::` names directly.
- Verification: generated C should be compiled for full verification and run when feasible.

## Still Open

### Generated C Target

1. Should generated C target C11 by default?
   - Current recommendation: yes. Use C11 for runtime convenience and compiler availability.

### Frontend Direction

2. Long term, do you want Dpp to become self-contained, or is being a Clang-based transpiler acceptable?

### Scope

3. Should Dpp focus on single-file transpilation first before multi-file/header builds?
   - Current recommendation: yes; single-file is the priority until the IR exists.

### Standard Library

4. Which tiny stdlib subset matters first: `array`, `span`, `string_view`, `vector`, or `string`?
   - Current working answer: `vector` is implemented; `string` is partially implemented. `string_view` and `span` are deferred.

### Output / Build

5. Should `dpp` emit one `.c` file plus required runtime files, or generate a small output directory with CMake/Make build glue?
   - Current recommendation: single `.c` + runtime first; output directory mode is the next step after the IR lands.

### Header Handling

6. When a user header is included from multiple `.cpp` files in a multi-file build,
   should `dpp` transpile it once to a shared `.dh` file, or inline it independently
   into each TU?
   - Current recommendation: inline today; `.dh` files once the build-directory model
     is in place. See `docs/architecture.md` — *Planned: Per-Header Transpiled Output*.

7. Should the preprocessor support `-I` search paths before the IR / multi-file work,
   or wait until both land together?
   - Current recommendation: `-I` is a self-contained CLI change and should come
     first — it unblocks multi-directory single-file test cases independently of the
     broader multi-file story.

### Name Mangling

8. How should the overload hash be computed? Options: (a) hash the full signature
   string, (b) use an incrementing counter per name (non-deterministic across builds),
   (c) always mangle all functions even without overloads.
   - Current recommendation: (a) hash the signature string; use a short prefix of a
     stable hash (e.g. FNV-1a, first 8 hex chars). Document the algorithm so it can
     be reproduced across rebuilds.
