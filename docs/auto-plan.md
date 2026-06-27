# Autonomous 10-Hour Plan

Started: 2026-06-26

## Architecture Assessment

Current state: 56 parity tests, sequential text-rewriting pipeline, no shared IR.
The roadmap calls for a Dpp IR later (after Clang bridge). Do NOT build half an IR
now — focus on features that work cleanly within the regex pipeline.

Key architectural gap to fix first: the `is_supported_include` whitelist in
`syntax_checker.cpp` blocks many valid C++ includes. Expanding it (and teaching
`lower_cpp_surface_types` to strip them) unlocks a large class of programs.

## Phase Checklist

### Phase 1 — Quick wins (Block 1)
- [x] `nullptr` → `NULL` in `lower_cpp_surface_types`
- [x] `constexpr` variable/function keyword → strip to `const`
- [x] `static_assert` → drop (already verified by C++ build)
- [x] Expand `is_supported_include`: `<cstddef>`, `<cstdlib>`, `<cstring>`, `<cmath>`,
      `<climits>`, `<utility>`, `<type_traits>`, `<functional>`, `<numeric>`, `<stdexcept>`
- [x] Strip those includes in `lower_cpp_surface_types`
- [x] Tests: 047_nullptr.cpp, 048_enum.cpp

### Phase 2 — Enum / enum class (Block 2)
- [x] New `lower_enums` pass in `basic.cpp` + declaration in `basic.h`
- [x] Plain `enum Foo { A, B }` → `typedef enum Foo { A, B } Foo;`
- [x] `enum class Foo { A, B }` → prefix enumerators (`Foo_A`, `Foo_B`) + typedef
- [x] Replace `Foo::A` → `Foo_A` for known enum class names
- [x] Insert pass before `lower_records` in `transpiler.cpp`
- [x] Tests: 048_enum.cpp, 049_enum_class.cpp

### Phase 3 — constexpr + default member initializers (Block 3)
- [x] Default member initializers in `records.cpp`
- [x] Tests: 050_default_member_init.cpp

### Phase 4 — Static class members (Block 4)
- [x] Static field lowering: emit `static T ClassName_field;` tentative definition
- [x] `ClassName::field` access from outside → `ClassName_field`
- [x] Static method lowering
- [x] Tests: 051_static_members.cpp

### Phase 5 — String methods expansion (Block 5)
- [x] `dpp_string_find_cstr`, `dpp_string_find_cstr_from`, `dpp_string_substr` in runtime
- [x] `.length()`, `.find(needle)`, `.find(needle, pos)`, `.substr(pos, len)` lowering
- [x] `std::size_t` → `size_t` in `lower_cpp_surface_types`
- [x] `size_t`/`std::size_t` → `%zu` in var tracker + `format_spec_for_type`
- [x] Fix double cleanup on return (use `close_scope_lines` in return path)
- [x] Tests: 052_string_methods.cpp

### Phase 6 — Range-based for on vectors (Block 6)
- [x] `for (const auto &x : vec)` and `for (auto x : vec)` lowering
- [x] Value copy for primitives, pointer for records
- [x] Tests: 053_range_for_vector.cpp

### Phase 7 — More algorithms (Block 7)
- [x] `std::find(v.begin(), v.end(), val) != v.end()` → `dpp_find_int`
- [x] `std::count(v.begin(), v.end(), val)` → `dpp_count_int`
- [x] `std::accumulate(v.begin(), v.end(), init)` → `dpp_accumulate_int`
- [x] Runtime `static inline` functions in `dpp_algorithm.h`
- [x] Tests: 054_algorithms.cpp

### Phase 8 — Map improvements (Block 8)
- [x] `m.find(key) != m.end()` → `dpp_map_contains`
- [x] `m.find(key) == m.end()` → `!dpp_map_contains`
- [x] `m.erase(key)` → `dpp_map_erase` / `dpp_unordered_map_erase`
- [x] Runtime: `dpp_map_erase`, `dpp_unordered_map_erase` in `dpp_map.{h,c}`
- [x] Tests: 055_map_find_erase.cpp

### Phase 9 — Architecture: shared scope utilities (Block 9)
- [x] Move `is_loop_kw` to `string_utils.h` / `string_utils.cpp`
- [x] Remove duplicate from `string.cpp` and `memory.cpp`

### Phase 10 — std::to_string (Block 10)
- [x] `dpp_string_from_int`, `dpp_string_from_long`, `dpp_string_from_double` in runtime
- [x] C11 `_Generic` macro `dpp_to_string(x)` for type dispatch
- [x] Lowering in `copy_decl_re` branch: `std::string s = std::to_string(n)` →
      `dpp_string s = dpp_to_string(n)`
- [x] Tests: 056_to_string.cpp

### Phase 11 — String index/push_back (Block 11)
- [ ] `s[i]` → `dpp_string_c_str(&s)[i]` (read-only access)
- [ ] `s.push_back(c)` → `dpp_string_append_char(&s, c)` (need runtime fn)
- [ ] `.at(i)` → `dpp_string_c_str(&s)[i]`
- [ ] Tests: 057_string_index.cpp

### Phase 12 — Free function string return (Block 12)
- [ ] Functions returning `std::string` → `dpp_string`
- [ ] Caller-side cleanup for returned `dpp_string` values
- [ ] Tests: 058_string_return.cpp

### Phase 13 — Documentation update (Block 13)
- [ ] Update `docs/unsupported.md` to reflect all new features
- [ ] Update `docs/roadmap.md` "Done" section
- [ ] Final commit

## Wakeup Strategy

Each block ends with `scripts/test_all.sh` green and a git commit.
`ScheduleWakeup` fires every ~270 s to stay in cache.
Check `docs/auto-plan.md` at start of each wakeup to know position.
