# Planned Feature Support

Features are drawn from what C++ developers broadly consider the most useful parts of
the language across C++11–C++26. They are grouped by standard version and annotated
with two things that matter for prioritization:

- **Transpile difficulty** — how hard it is for dpp to lower the feature to C.
- **User reception** — whether developers appreciate the *interface* (the syntax / API
  they write), the *implementation* (the underlying mechanics the compiler or runtime
  provides), or both. Many features have a loved interface sitting on top of a
  mechanism that developers find confusing, broken, or painful to replicate.

---

## C++11

### `auto` type deduction
**Transpile difficulty:** Easy — replace `auto` with the inferred concrete type;
requires type-tracking but no structural transformation.
**User reception:** Interface universally loved — eliminates verbose type repetition.
Implementation (template argument deduction rules) is considered complex but hidden
from most users.

### Range-based `for`
**Transpile difficulty:** Moderate — must expand to begin/end iterator pairs or index
loops depending on the container type.
**User reception:** Interface universally loved. The underlying iterator protocol
(`begin()`/`end()`) is considered clunky but invisible in normal use.

### Lambda expressions
**Transpile difficulty:** Hard — must generate a capture struct and a corresponding
function, then wire call sites through it.
**User reception:** Interface widely appreciated for inline callbacks and algorithms.
Developers criticize the capture syntax (`[=]`, `[&]`, explicit lists) as a source of
subtle bugs. The implementation (compiler-generated functor) is opaque and makes error
messages hard to read.

### `nullptr`
**Transpile difficulty:** Trivial — replace with `NULL` or `(void*)0`.
**User reception:** Universally appreciated. No complaints about interface or
implementation.

### `constexpr` (basic)
**Transpile difficulty:** Hard — constant-folding at transpile time requires expression
evaluation. Simple integer constants are tractable; recursive or template-driven
constexpr is not.
**User reception:** Interface loved for expressing compile-time intent. Users complain
that the rules for what is and isn't `constexpr` changed substantially between
standards, making the feature feel inconsistent.

### Initializer lists / aggregate initialization
**Transpile difficulty:** Moderate — already partially done; general
`std::initializer_list<T>` requires synthetic arrays.
**User reception:** Interface loved (`= {1, 2, 3}` syntax). Implementation surprises
developers when narrowing conversions are silently allowed or rejected depending on
context.

### `static_assert`
**Transpile difficulty:** Easy — emit a C11 `_Static_assert` or `#error` for the
constant-false case.
**User reception:** Interface universally loved. No significant complaints.

### Scoped enumerations (`enum class`)
**Transpile difficulty:** Easy — prefix every enumerator with the enum name; wrap
arithmetic casts.
**User reception:** Interface appreciated for preventing accidental implicit
conversions. Old-style `enum` behavior is widely considered a design mistake.

### Variadic templates
**Transpile difficulty:** Very hard — requires monomorphizing each pack expansion at
call sites, similar to function template handling but combinatorially more complex.
**User reception:** Interface appreciated for eliminating C-style varargs. The syntax
(`...`, `sizeof...`, fold expressions) has a steep learning curve and is considered one
of the harder parts of modern C++.

### Move semantics / rvalue references
**Transpile difficulty:** Very hard — correct lowering requires value-category tracking
through every expression. Without it, moves degrade to copies, which changes program
behavior for non-trivial types.
**User reception:** Interface polarizing — developers who understand it consider it
essential for performance; beginners find `&&`, `std::move`, and `std::forward`
deeply confusing. The implementation (destructor-safe transfer of ownership) is
considered clever but error-prone.

---

## C++14

### Generic lambdas (`auto` parameters)
**Transpile difficulty:** Hard — each `auto` parameter is a template parameter;
requires per-call-site instantiation of the generated functor.
**User reception:** Interface appreciated as a natural extension of lambdas.

### `std::make_unique`
**Transpile difficulty:** Easy — already covered by the `unique_ptr` lowering; `make_unique`
is a thin wrapper.
**User reception:** Interface universally loved as the safe way to construct
`unique_ptr`. The prior pattern (`new` inside a constructor argument) was a well-known
exception-safety footgun.

---

## C++17

### Structured bindings
**Transpile difficulty:** Moderate — decompose aggregate or pair/tuple into named
locals; requires knowing the shape of the decomposed type.
**User reception:** Interface loved for range-for over maps and tuple returns.
Implementation (the binding rules differ for arrays, structs, and tuple-like types)
surprises developers.

### `if constexpr`
**Transpile difficulty:** Hard — requires evaluating the condition at transpile time and
emitting only the taken branch, which demands type information at each call site.
**User reception:** Interface loved — cleanly replaces SFINAE tricks for conditional
template code. SFINAE itself is widely considered one of C++'s worst interfaces.

### `std::optional<T>`
**Transpile difficulty:** Moderate — lower to a tagged struct `{ T value; bool has_value; }`.
**User reception:** Interface universally loved for expressing nullable values without
pointers. No meaningful complaints about the interface; the underlying in-place storage
trick (avoiding heap allocation) is appreciated.

### `std::variant<Ts...>`
**Transpile difficulty:** Hard — requires a discriminated union with per-type
constructors/destructors and `std::visit` dispatch.
**User reception:** Interface appreciated as a type-safe union. `std::visit` with
overloaded lambdas is widely considered ugly syntax; many developers use helper
libraries on top of it.

### `std::string_view`
**Transpile difficulty:** Moderate — lower to a `{ const char *data; size_t len; }`
struct with matching helper functions.
**User reception:** Interface loved for avoiding string copies. Developers frequently
encounter lifetime bugs (dangling views) which are a common criticism.

### Fold expressions
**Transpile difficulty:** Hard — requires recursive code generation over a variadic
pack. Depends on variadic template support.
**User reception:** Interface appreciated for collapsing variadic boilerplate.
Considered an expert-level feature with confusing syntax.

### Class template argument deduction (CTAD)
**Transpile difficulty:** Hard — requires deduction-guide resolution at each
construction site.
**User reception:** Interface loved (`std::pair p{1, 2}` vs `std::make_pair`).
Deduction guide rules are considered one of the most complex parts of modern C++.

---

## C++20

### Concepts
**Transpile difficulty:** Extremely hard — concepts constrain template arguments; the
transpiler would need to verify constraints at monomorphization time and produce useful
errors.
**User reception:** Interface universally loved — concepts transform unreadable
172-line template errors into single-line diagnostics. The subsumption and partial
ordering rules are complex for library authors but transparent to most users.

### Ranges and views
**Transpile difficulty:** Extremely hard — a pipeline like `v | filter | transform |
take` expands to deeply nested lazy iterator types; correct lowering requires full
type knowledge.
**User reception:** Interface loved for composable, readable data pipelines. The
underlying implementation (concepts-constrained sentinel/range adaptors) is notorious
for compilation times and cryptic errors when something goes wrong. The interface is
praised; the implementation mechanics are widely criticized.

### Coroutines (`co_await`, `co_yield`, `co_return`)
**Transpile difficulty:** Extremely hard — coroutines compile to state machines; a
correct lowering requires generating the full coroutine frame struct and suspension
points.
**User reception:** Interface appreciated for async and generator patterns. Developers
widely criticize the lack of standard library support — the language gives the
machinery but almost no ready-to-use building blocks. Library support (e.g.,
`std::generator` in C++23) improved this later.

### Three-way comparison / spaceship operator (`<=>`)
**Transpile difficulty:** Moderate — emit `<`, `==`, `>` comparisons from a single
generated comparison function.
**User reception:** Interface appreciated for eliminating comparison boilerplate.
The default `= default` generation is loved; the `std::strong_ordering` /
`std::partial_ordering` type hierarchy is considered overly complex.

### `std::format` / `std::print`
**Transpile difficulty:** Moderate — lower to `snprintf` / `printf` for simple format
strings. Dynamic format strings or custom formatters are harder.
**User reception:** Interface universally loved as a safe replacement for `printf` and
a readable alternative to `iostream`. No significant complaints about the interface.

### Designated initializers
**Transpile difficulty:** Easy — already legal in C99; the transpiler must map C++
designated initializer syntax to the C form and reorder fields if necessary.
**User reception:** Interface loved — familiar to anyone who has used C99. No
significant complaints.

---

## C++23

### `std::expected<T, E>`
**Transpile difficulty:** Moderate — lower to a tagged union struct with `{ T value; E
error; bool ok; }` and matching accessors.
**User reception:** Interface loved as a better alternative to error codes and
exceptions for recoverable errors. Developers who come from Rust compare it favorably
to `Result<T, E>`. No significant implementation complaints.

### `std::flat_map` / `std::flat_set`
**Transpile difficulty:** Moderate — sorted parallel arrays with binary search; more
tractable than tree-based `std::map`.
**User reception:** Interface appreciated for cache-friendly performance.
Developers note that invalidation rules (iterators invalidated on insert) require care.

### Deducing `this`
**Transpile difficulty:** Hard — the explicit `self` parameter must be emitted as
an ordinary function parameter, which dpp already does for methods, but CRTP
elimination and recursive lambda patterns add complexity.
**User reception:** Interface loved by library authors for eliminating CRTP boilerplate.
Most application developers consider it a niche feature.

### `std::generator<T>`
**Transpile difficulty:** Extremely hard — depends on coroutine support.
**User reception:** Interface loved — fills the gap that coroutines left by not
providing standard generator types. Library-author reception is very positive.

---

## C++26

### Static reflection
**Transpile difficulty:** Extremely hard — requires the transpiler to expose type
metadata (field names, types, function signatures) as first-class values at compile
time.
**User reception:** Interface is considered one of the most anticipated C++ features in
years. Developers expect it to eliminate enormous volumes of hand-written serialization,
ORM, and registration boilerplate. No reception yet for the implementation — the
feature is new.

### Contracts (`pre:`, `post:`, `assert:`)
**Transpile difficulty:** Hard — pre/post conditions lower to runtime assertion macros;
the build-mode selection (ignore / observe / enforce) requires a compile-time flag.
**User reception:** Interface widely anticipated for safety and documentation. Considered
the most impactful safety feature in C++26. No major criticism of the interface design;
the long standardization process (multiple redesigns) is noted as a sore point.

### Pattern matching (`inspect`)
**Transpile difficulty:** Very hard — requires generating a chain of type tests and
destructuring code equivalent to a `switch` over discriminated unions.
**User reception:** Interface universally anticipated — developers consistently cite
pattern matching as one of the most requested C++ features, pointing at Rust, Swift,
and Haskell as references. No implementation in compilers yet at time of writing so
reception of the specifics is still forming.

### Pack indexing and expansion statements
**Transpile difficulty:** Hard — requires tracking pack sizes and emitting loop-unrolled
code at transpile time.
**User reception:** Interface appreciated as a long-overdue quality-of-life improvement
for template metaprogramming. Considered expert-level; most developers would encounter
it through library code rather than writing it directly.

---

## Notes on Scope

Features that are intentionally out of scope (exceptions, virtual dispatch, RTTI,
multiple inheritance, the full Ranges library) are documented in
[docs/unsupported.md](unsupported.md).

The above list is aspirational and ordered loosely by expected implementation
priority. Not all features will be implemented; the goal is to cover the subset that
appears most commonly in real single-binary C++ programs.
