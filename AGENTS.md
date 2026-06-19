# AGENTS.md - DummyCpp

This project is building **DummyCpp**, shorthand **Dpp**: a small C++ to C transpiler written in C++.

## Product Direction

- Start small and make real programs transpile to C and compile.
- Prefer working transformations over complete C++ syntax validation in early milestones.
- Ignore difficult C++ portions initially; document gaps clearly and continue building useful coverage.
- Progressively add more of the C++ standard library by lowering it to C helpers/runtime code.
- Use open source resources when possible: existing parsers, compiler frontends, ABI notes, libc/libstdc++/LLVM/Clang references, tiny C++ interpreters/transpilers, and test suites when licenses permit.
- Keep generated C readable enough to inspect, but prefer compact output when there is a tradeoff.
- Objects/classes should lower to C structs with no hidden object overhead beyond fields and explicit helper functions.
- Favor zero-cost or explicit-cost translations. If a feature needs runtime support, make that support visible and documented.

## Early Technical Bias

- Use C++ for the transpiler implementation.
- Use a Clang-based frontend for now. Clang/libTooling/libclang dependencies are acceptable if they get real C++ parsing moving faster.
- Target C++11 source support first.
- First examples should be small.
- First target should be a constrained C++11 subset:
  - primitive types, pointers, references where simple,
  - functions,
  - structs/classes with fields and simple methods,
  - constructors/destructors only when they can be lowered explicitly,
  - namespaces as name mangling,
  - simple templates only after non-template code works,
  - a tiny selected `std::` namespace standard-library subset implemented in Dpp runtime headers/sources.
- Delay:
  - exceptions,
  - RTTI,
  - multiple/virtual inheritance,
  - coroutines,
  - concepts,
  - full template metaprogramming,
  - full overload resolution,
  - perfect standard-library compatibility.

## Working Rules

- Keep the main README.md presentable, clean, and readable.
- Keep notes local to where they are applicable.
  - For concise notes for a subdirectory read/write to README.md.
  - For code specific notes write concise comments.
- Always use trash instead of `rm`.
- Keep larger project notes in `docs/`.
- Put condensed open questions only in `docs/questions.md`.
- When a decision is made, move it from questions into `docs/decisions.md`.
- Prefer small examples in `examples/` and regression tests in `tests/`.
- Do not block on theoretical completeness; build the narrowest compileable vertical slice first.
- Whenever adding a supported C++ feature, add:
  - one input example,
  - concise expected generated C shape notes without large snippets,
  - full verification by compiling generated C, and running it when feasible.

## Naming

- Project name: `DummyCpp`.
- Tool shorthand: `Dpp`.
- Working CLI name: `dpp`.

## Directories

- `src/` - transpiler implementation. See nested `AGENTS.md` files before editing parser or converter code.
- `include/` - public Dpp C++ headers matching implementation modules.
- `inject/` - reusable plain-C runtime ports for supported C++ classes/features, built by CMake as `dpp_inject` and linked with generated-C executables.
- `tests/` - parity and unsupported-feature fixtures for the bootstrap subset.
- `docs/` - longer-lived project notes, decisions, plans, references, and open questions.
