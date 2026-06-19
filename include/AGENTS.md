# AGENTS.md - Public Headers

Scope: public C++ headers under `include/`.

- Keep headers minimal, self-contained, and aligned with their implementation modules.
- Use `#pragma once` consistently.
- Place exported APIs under the `dpp` namespace and module APIs under nested namespaces such as `dpp::convert`.
- Avoid exposing implementation-only helpers; keep those in anonymous namespaces in `.cpp` files.
- Update consumers and CMake targets when adding public modules.

