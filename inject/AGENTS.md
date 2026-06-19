# AGENTS.md - Injected Runtime

Scope: reusable C runtime code under `inject/`.

- Keep runtime code plain C, feature-scoped, and reusable by generated C programs.
- Name runtime symbols with a `dpp_` prefix to avoid collisions in generated executables.
- Keep explicit-cost behavior visible: allocation, bounds checks, abort paths, and cleanup should be easy to inspect.
- Update `inject/README.md` when adding runtime support for a new lowered C++ feature.
- Update `inject/CMakeLists.txt` when adding runtime source files.
- Validate runtime changes through `scripts/test_all.sh` when feasible.

