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
- First examples should be small.
- Generated C should be readable, but compactness is preferred when readability and compactness conflict.
- Generated C should be compiled for full verification; run resulting binaries when feasible.
- Standard library support will be added progressively through small Dpp runtime helpers that support selected `std::` namespace features directly.
- Open source resources should be used heavily as references, with license notes for any imported material.
