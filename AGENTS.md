# AGENTS.md - DummyCpp

This project is building **DummyCpp**, shorthand **Dpp**: a C++ to C transpiler written in C++.

## Product Direction

- Use C++ for the dpp transpiler implementation.
- Prefer working transformations over complete C++ syntax validation in early milestones.
- Ignore difficult C++ portions initially; document gaps clearly but concisely in `docs/unsupported.md` and continue building useful coverage.
- Progressively add more of the C++ standard library by lowering it to C helpers/runtime code.
- Try to keep generated C readable enough to inspect.
- Objects/classes should lower to C structs with no hidden object overhead beyond fields and explicit helper functions.
- Favor zero-cost or explicit-cost translations. If a feature needs runtime support, make that support visible and documented.
- Place c++ std lib replacements in `inject/`. Sometimes for things like templates this may need to be macros.
- dpp client tool will use similar flags to gcc.
- It is not your primary job to detect syntax errors.
- All runtime c++ features are to be handled by the compiler. This requires much better memory tracking and function pointer tracking.

## Planning

- When the user asks to “plan”, “plan out”, or “present a plan”, provide the plan in the current client session by default.
- Be thorough in your thinking. Compilers are a complex topic.

## Transpiler

- We will exactly match the c++ standard for now. 
- the prepocessor will do nothing for now.
- the lexer is re2c.
- the parser will be handwritten:
  - recursive descent
  - Pratt expressions

## Early Technical Bias

- Target C++11 standard support first.
- Most syntax errors will be ignored.
- Access modifiers will be ignored, everything is public in c.
- Ignore barely used, overly complicated c++ features. 
- Delay:
  - exceptions,
  - RTTI,
  - multiple/virtual inheritance,
  - coroutines,
  - concepts,
  - full template metaprogramming,
  - full overload resolution,
  - perfect standard-library compatibility.

## Testing

- Prefer regression tests in `tests/`.
- The primary test is a diff between the c++ executable and the c executable printouts.
- Use assertions for more exhaustive tests in place of printouts. 
- Keep the first <10 tests small to ensure testabillity, in order of importance this is cout, arithmetic, and assertions.
- After the basics are done the test files should be comprehensive. A test of <vector> should touch all aspects of the c++ standard.
- Run test with `scripts/test_all.sh`
- Multifile tests will go into `examples`.

## Working Rules

- Keep the main README.md presentable, clean, and readable.
- Keep notes local to where they are applicable.
- When skipping complex functionality or writing suboptimal code write a TODO comment in the appropriate file. 
- Always use `trash` instead of `rm`.
- Keep larger project notes in `docs/`.
- Put condensed open questions only in `docs/questions.md`.
- When a decision is made, move it from questions into `docs/decisions.md`.
- Place common string utilities in `include/dpp/string_utils.h`

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
