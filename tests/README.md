# DummyCpp Test Harness

The primary test oracle is behavioral parity:

1. Compile the original C++11 test case.
2. Run it and capture stdout + exit status.
3. Transpile it with `dpp`.
4. Compile the generated C.
5. Run the generated-C binary and capture stdout + exit status.
6. Diff original C++ behavior against generated-C behavior.

Run all tests with `scripts/test_all.sh`.

Each supported test case lives in `tests/cases/*.cpp`. The numeric prefixes keep the
smallest required features first: `cout`/`printf`, arithmetic, assertions, then
records/classes and runtime-backed containers. The later cases are intentionally longer
and combine helpers, records/classes, vectors, loops, and multiple return paths.

Unsupported-feature fixtures live in `tests/unsupported/*.cpp`. Those pass when `dpp`
rejects them with a syntax/support diagnostic.
