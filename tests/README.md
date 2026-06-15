# DummyCpp Test Harness

The primary test oracle is behavioral parity:

1. Compile the original C++11 test case.
2. Run it and capture stdout + exit status.
3. Transpile it with `dpp`.
4. Compile the generated C.
5. Run the generated-C binary and capture stdout + exit status.
6. Diff original C++ behavior against generated-C behavior.

Run all tests:

```bash
scripts/test_all.sh
```

Each test case lives in `tests/cases/*.cpp`.

