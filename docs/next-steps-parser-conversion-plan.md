# DummyCpp Next Steps

## Current Rule

Move by verified vertical slices:

1. Add or extend one C++ test case.
2. Transpile it with `dpp`.
3. Compile the generated C.
4. Run both binaries.
5. Compare stdout and exit status.

Do not widen a feature until `scripts/test_all.sh` is green.

## Current Support Snapshot

- C++11-friendly input subset.
- `std::cout` / `cout` chains lowered to compact `printf` calls.
- Free functions, primitive locals, loops, and simple branches that are already valid C.
- Struct/class fields and one-line methods lowered to `Type_method(self, ...)`.
- Simple constructors lowered to `Type_init`.
- Single and multiple non-virtual inheritance lowered as embedded base fields,
  with access modifiers ignored and base constructor calls, base method calls,
  and base field access rewritten through embedded base members.
- `std::vector<int>`, `std::vector<double>`, and vector of simple record/class values through `inject/c/dpp_vector.*`.
- Runtime cleanup before vector-return paths in supported local-function shapes.
- Unsupported diagnostics for unsupported headers, exceptions, virtual inheritance, virtual methods, and user templates.

## Current Test Layout

- `001`-`011`: small focused feature tests.
- `012_long_records_classes.cpp`: condensed record/class workflow coverage.
- `013_long_vectors_runtime.cpp`: condensed vector/runtime workflow coverage.
- `015_single_inheritance.cpp`: first single-inheritance coverage.
- `023_inheritance_access_modifiers.cpp`: multiple inheritance with ignored access modifiers.
- `tests/unsupported/`: expected rejection cases.

The newer tests should stay condensed. Prefer adding sections/functions to the long cases unless a new feature needs a small focused bootstrap test first.

## Near-Term Work

1. Add explicit parser/keyword unit tests.
2. Move more conversion logic from regex/string passes toward token-aware or IR-backed transforms.
3. Start a small Dpp IR for records, functions, calls, expressions, and runtime needs.
4. Add a Clang AST bridge that extracts only supported constructs into Dpp IR.
5. Improve `cout` type handling with actual expression/type tracking.
6. Support `push_back(function_returning_record())` by emitting a temporary before the push.
7. Extend inheritance:
   - derived-to-base value passing,
   - pointer/reference base views,
   - inherited method calls from inside derived method bodies.
8. Add diagnostics for overload collisions, unsupported vector element types, lambdas, and references-as-fields.

## Non-Goals For Now

- Virtual inheritance.
- Full access control.
- Virtual dispatch/vtables.
- Exceptions.
- Full template support.
- Full standard-library compatibility.
- Multi-file builds.
