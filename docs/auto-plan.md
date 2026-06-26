# Autonomous 10-Hour Plan

Started: 2026-06-26

## Architecture Assessment

Current state: 46 parity tests, sequential text-rewriting pipeline, no shared IR.
The roadmap calls for a Dpp IR later (after Clang bridge). Do NOT build half an IR
now — focus on features that work cleanly within the regex pipeline.

Key architectural gap to fix first: the `is_supported_include` whitelist in
`syntax_checker.cpp` blocks many valid C++ includes. Expanding it (and teaching
`lower_cpp_surface_types` to strip them) unlocks a large class of programs.

## Phase Checklist

### Phase 1 — Quick wins (Block 1)
- [x] `nullptr` → `NULL` in `lower_cpp_surface_types`
- [ ] `constexpr` variable/function keyword → strip to `const` / nothing
- [ ] `static_assert(expr, msg)` → `_Static_assert(expr, msg)` in `lower_assertions`
- [ ] Expand `is_supported_include`: `<cstddef>`, `<cstdlib>`, `<cstring>`, `<cmath>`,
      `<climits>`, `<utility>`, `<type_traits>`, `<functional>`, `<numeric>`
- [ ] Strip those includes in a new `lower_c_compat_includes` pass (no C equivalent)
- [ ] Tests: 047_nullptr.cpp, 047b_static_assert.cpp, 047c_constexpr.cpp

### Phase 2 — Enum / enum class (Block 2)
- [ ] New `lower_enums` pass in `basic.cpp` + declaration in `basic.h`
- [ ] Plain `enum Foo { A, B }` → `typedef enum Foo { A, B } Foo;`
- [ ] `enum class Foo { A, B }` → prefix enumerators (`Foo_A`, `Foo_B`) + typedef
- [ ] Replace `Foo::A` → `Foo_A` for known enum class names
- [ ] Insert pass before `lower_records` in `transpiler.cpp`
- [ ] Tests: 048_enum.cpp, 049_enum_class.cpp

### Phase 3 — constexpr + default member initializers (Block 3)
- [ ] Default member initializers in `records.cpp`:
      `int x = 5;` in class body → `self->x = 5;` in generated `_init`
      when not covered by explicit constructor initializer list
- [ ] `constexpr` stripping (already covered in Phase 1)
- [ ] Tests: 050_default_member_init.cpp

### Phase 4 — Static class members (Block 4)
- [ ] `static int count;` in class body → collect in `records.cpp`
- [ ] Emit `static T ClassName_field;` as a global before the struct typedef
- [ ] `ClassName::field` → `ClassName_field` in `lower_cpp_surface_types` or records
- [ ] `static` method lowering (remove `static`, promote to free function)
- [ ] Tests: 051_static_members.cpp

### Phase 5 — String methods expansion (Block 5)
- [ ] `dpp_string_find`, `dpp_string_substr`, `dpp_string_length` in runtime
- [ ] `dpp_string_insert`, `dpp_string_erase` in runtime
- [ ] `DPP_STRING_NPOS` constant → `SIZE_MAX`
- [ ] `std::string::npos` → `DPP_STRING_NPOS`
- [ ] Lowering in `string.cpp` for all new methods
- [ ] Tests: 052_string_find.cpp, 053_string_substr.cpp

### Phase 6 — Range-based for on vectors (Block 6)
- [ ] Extend `lower_vectors` to detect `for (const auto &x : vec)` and
      `for (auto x : vec)` and `for (T x : vec)` when `vec` is a known vector
- [ ] Expand to index-loop with element access via `dpp_vector_const_at`/`dpp_vector_at`
- [ ] Tests: 054_range_for_vector.cpp

### Phase 7 — More algorithms (Block 7)
- [ ] `std::find(v.begin(), v.end(), val)` → runtime `dpp_vector_find_*`
- [ ] `std::count(v.begin(), v.end(), val)` → runtime `dpp_vector_count_*`
- [ ] `std::accumulate(v.begin(), v.end(), init)` → runtime helper
- [ ] Tests: 055_algorithm_find.cpp

### Phase 8 — Map improvements (Block 8)
- [ ] `m.find(key) != m.end()` → `dpp_map_contains(&m, &(T){key})`
- [ ] `m.find(key) == m.end()` → `!dpp_map_contains(&m, &(T){key})`
- [ ] `m.erase(key)` → `dpp_map_erase(&m, &(T){key})`
- [ ] Runtime: `dpp_map_erase`, `dpp_unordered_map_erase`
- [ ] Tests: 056_map_find_erase.cpp

### Phase 9 — Architecture: shared scope utilities (Block 9)
- [ ] Extract `is_loop_kw`, `count_char`, `leading_indent`, `peek_*_lines`
      into `include/dpp/scope_utils.h` / `src/scope_utils.cpp`
- [ ] De-duplicate between `memory.cpp` and `string.cpp`
- [ ] Verify all tests still pass after refactor

### Phase 10 — Documentation update (Block 10)
- [ ] Update `docs/unsupported.md` to reflect all new features
- [ ] Update `docs/roadmap.md` "Done" section
- [ ] Final commit

## Wakeup Strategy

Each block ends with `scripts/test_all.sh` green and a git commit.
`ScheduleWakeup` fires every ~270 s to stay in cache.
Check `docs/auto-plan.md` at start of each wakeup to know position.
