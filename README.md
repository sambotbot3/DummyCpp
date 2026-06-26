# DummyCpp (dpp)

A C++11-to-C transpiler. Lowers a growing subset of C++ to readable C, compiles it with a plain C compiler, and verifies correctness by diffing output against the native C++ build.

## Quickstart

Install dependencies (Ubuntu/Debian):
```bash
scripts/install_dependencies_ubuntu.sh
```

Build:
```bash
scripts/build.sh
```

Run the full parity test harness:
```bash
scripts/test_all.sh
```

Transpile a single file manually:
```bash
build/dpp tests/cases/001_cout.cpp -o /tmp/out.c
cc /tmp/out.c -I inject/c build/inject/libdpp_inject.a -o /tmp/out && /tmp/out
```

## Docs

| Document | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Pipeline overview and project layout |
| [docs/roadmap.md](docs/roadmap.md) | Planned features and priorities |
| [docs/CONVENTIONS.md](docs/CONVENTIONS.md) | Code and contribution conventions |
| [docs/unsupported.md](docs/unsupported.md) | Intentionally unsupported C++ features |
| [docs/decisions.md](docs/decisions.md) | Resolved design decisions |
