# AGENTS.md - Transpiler Source

Scope: implementation under `src/`.

- Keep the bootstrap pipeline simple and explicit in `src/transpiler.cpp`.
- Prefer small, feature-scoped lowering passes over one large conversion step.
- Add public declarations in `include/dpp/` when adding source modules or exported types.
- Keep generated C readable and deterministic; avoid hidden runtime behavior.
- Update the relevant `CMakeLists.txt` when adding or moving implementation files.
- After source changes, prefer validating with `scripts/test_all.sh`.

