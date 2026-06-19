# AGENTS.md - Tests

Scope: regression fixtures and test documentation under `tests/`.

- Supported cases live in `tests/cases/` and are checked by behavioral parity: C++11 output/status must match generated-C output/status.
- Unsupported cases live in `tests/unsupported/` and should fail with a syntax/support diagnostic.
- When adding a supported feature, include a focused case and compile/run verification through `scripts/test_all.sh`.
- Keep test programs small but realistic; combine features only when testing integration behavior.
- Update `tests/README.md` if the harness behavior or fixture layout changes.

