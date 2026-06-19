# AGENTS.md - Parser

Scope: tokenizer, parser façade, and syntax/support diagnostics under `src/parser/`.

- The checked-in lexer source is `lexer.re`; regenerate `lexer.cpp` with `scripts/gen_lexer.sh` after editing it.
- Keep `lexer.cpp` generated and avoid hand-editing it unless the generator path is also updated.
- Treat parser work as bootstrap-friendly: preserve the original source text and collect enough tokens for diagnostics/lowering.
- Put unsupported-feature checks in syntax checking rather than letting conversion fail obscurely.
- Keep diagnostics actionable and stable enough for `tests/unsupported/` fixtures.

