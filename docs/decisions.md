# DummyCpp Decisions

## 2026-06-15

- Project name: **DummyCpp**.
- Short name / tool shorthand: **Dpp**.
- Working CLI name: `dpp`.
- Implementation language: C++.
- Source language target: C++11 first.
- Frontend direction: Clang-based is good for now.
- Initial priority: make friendly small C++ inputs transpile to C and compile.
- Syntax checking and full validation are lower priority than producing working output.
- Early object model: C++ objects/classes lower to plain C structs plus explicit helper functions.
- Difficult C++ features are delayed and documented instead of blocking the project.
- Generated C should be readable, but compactness is preferred when readability and compactness conflict.
- Generated C should be compiled for full verification; run resulting binaries when feasible.
- Primary test oracle: compare original C++ stdout + exit status against generated-C stdout + exit status.
- `std::cout`/printing support should come early because it makes the behavioral test harness useful.
- Standard library support will be added progressively through small Dpp runtime helpers that support selected `std::` namespace features directly.
- Open source resources should be used heavily as references, with license notes for any imported material.
- Parser and syntax checks live in their own library (`dpp_parser`) with its own CMake file.
- The bootstrap lexer is maintained as `src/parser/lexer.re` for re2c `4.5.1`; generation is explicit through `scripts/gen_lexer.sh`, and CMake builds the generated `lexer.cpp`.
- Conversion passes live behind focused headers under `include/dpp/convert/`, such as `include/dpp/convert/iostream.h`.
- Public implementation APIs use the `dpp` namespace.
- Reusable C runtime/helper code lives under `inject/`; `std::vector` support uses a direct C runtime port built as `dpp_inject` and linked into generated-C executables.
- `std::unique_ptr<T>` and `std::shared_ptr<T>` lower to explicit C ownership helpers in `dpp_memory`, with generated cleanup before return and function-scope exit.
