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

1. Should generated C target C11 by default?

## Frontend Direction

2. Long term, do you want Dpp to become self-contained, or is being a Clang-based transpiler acceptable?

## Scope

3. Is the first useful target "small C++ programs with structs, methods, constructors, and tiny stdlib"?
4. Should Dpp focus on single-file transpilation first before multi-file/header builds?

## Standard Library

5. Which tiny stdlib subset matters first: `array`, `span`, `string_view`, `vector`, or `string`?

## Output / Build

6. Should `dpp` emit one `.c` file plus required runtime files, or generate a small output directory with CMake/Make build glue?
