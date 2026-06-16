# DummyCpp Notes

## Working Philosophy

DummyCpp should be useful before it is complete. The project should prefer a narrow path that transpiles and compiles over a broad parser that rejects everything interesting.

Early Dpp can assume friendly C++11 input. It can produce helpful diagnostics eventually, but the first measure of success is: can a small C++ program become C, compile, and run?

## Translation Model

- C++ structs/classes become C `struct` types.
- Methods become free C functions with explicit `self`/`this` parameters.
- Constructors become explicit init functions.
- Destructors become explicit destroy functions.
- Namespaces/classes are lowered through deterministic C-safe names.
- Runtime helpers live in Dpp-owned C headers/sources.
- Selected standard-library support should accept `std::` names directly instead of requiring users to write Dpp-specific namespace names.

## Parser / Frontend Options

Potential approaches:

1. Use Clang AST/libTooling first.
   - Fastest path to real C++ parsing.
   - Lets Dpp focus on lowering instead of parsing.
   - Downside: heavy dependency and may hide hard frontend questions.

2. Use libclang C API.
   - Easier embedding boundary.
   - Less detailed than full Clang AST in some areas.

3. Write a tiny subset parser.
   - Keeps Dpp self-contained.
   - Good for learning and constrained examples.
   - Risk: can stall before useful programs compile.

Initial direction: use Clang resources for the first useful lowerer, while documenting which AST features we depend on. Revisit later if we want a self-contained frontend.

## Generated C Style

- Prefer C11 unless a reason appears for C99 or C23. This is still open.
- Emit includes explicitly.
- Emit readable helper function names.
- Keep output readable enough to inspect, but compact output wins when there is a tradeoff.
- Avoid clever macro-heavy output in early stages unless it materially reduces generated-code bloat.
- Avoid hidden heap allocation unless the source feature clearly requires it.

## Verification

- Every supported feature should have at least one generated-C compile check.
- Prefer end-to-end tests: C++ input -> generated C -> C compiler -> executable -> expected exit code/output.

## Runtime Direction

The runtime should be boring C:

- reusable snippets under `inject/`
- optional per-feature headers like `dpp_array.h`, `dpp_string_view.h`, `dpp_vector.h`

Keep helpers small enough that generated C remains inspectable.
