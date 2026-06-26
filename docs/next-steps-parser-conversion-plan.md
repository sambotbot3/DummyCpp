# DummyCpp Next Steps

## Working Rule

Move by verified vertical slices:

1. Add or extend one test case in `tests/cases/`.
2. Transpile it with `dpp`.
3. Compile the generated C.
4. Run both binaries.
5. Compare stdout and exit status via `scripts/test_all.sh`.

Do not widen a feature until `scripts/test_all.sh` is green.

## Immediate Work (ordered by priority)

1. **`-I` search paths** — add `-I <dir>` to the CLI and thread a path list through
   `PreprocessContext` in `preprocessor.cpp`. Resolution order: relative to including
   file, then each `-I` dir in order.

2. **Name mangling** — implement overload-hash mangling for `lower_records` and the
   free-function pass. Use FNV-1a over the full signature string; emit the first 8 hex
   chars as a suffix. Single-overload functions are not mangled.

3. **Token-aware `cout` pass** — replace the regex-based iostream lowering with a
   pass that walks the token list, so more expression forms are handled correctly.

4. **Dpp IR skeleton** — define `DppRecord`, `DppFunction`, and `DppExpr` in a new
   `include/dpp/ir.h`. Populate from the token stream for structs and free functions.
   No Clang dependency in this step.

5. **Scope-exit cleanup** — extend the lifetime-tracking in `lower_memory`,
   `lower_strings`, and `lower_vectors` to emit cleanup at all exits from a block,
   not just at `return` statements.

## Non-Goals For Now

- Virtual inheritance, virtual dispatch, vtables.
- Full access control enforcement.
- Exceptions, RTTI, coroutines, concepts.
- Full template instantiation / SFINAE / variadic templates.
- Full standard-library compatibility.
- Per-header `.dh` transpilation (comes after IR).
