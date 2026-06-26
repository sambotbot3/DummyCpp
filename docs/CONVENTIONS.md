# Dpp Lowering Conventions

These are the canonical rules for how C++ constructs map to C in generated output.
Every lowering pass should follow these conventions. The guiding principle is
minimalism: generated C should look like something a careful human might write.

---

## Namespaces

Prepend the namespace name with `_` as a separator. Nested namespaces concatenate.

```cpp
// C++
namespace abc {
    int var;
    void foo();
}
```
```c
// C
int abc_var;
void abc_foo(void);
```

`using namespace std;` is ignored at the lowering layer; `std::` prefixes are
stripped by the conversion passes that recognize each supported symbol.

---

## Classes and Structs

Access modifiers (`public`, `private`, `protected`) are ignored — everything becomes
public in C.

```cpp
// C++
class Foo {
public:
    int x;
private:
    int y;
};
```
```c
// C
typedef struct Foo {
    int x;
    int y;
} Foo;
```

Use the combined `typedef struct Foo { ... } Foo;` form so the type name works
without a `struct` tag everywhere.

---

## Methods

Instance methods become free functions with an explicit `self` pointer as the first
parameter. The generated name is `TypeName_methodName`.

```cpp
// C++
struct Point {
    int x, y;
    int sum() const { return x + y; }
};
```
```c
// C
typedef struct Point { int x; int y; } Point;
int Point_sum(const Point *self) { return self->x + self->y; }
```

---

## Constructors and Destructors

Constructors lower to `TypeName_init`. Destructors lower to `TypeName_destroy`.
Both take `TypeName *self` as first parameter. Initializer lists become field
assignments inside the init body.

```cpp
// C++
struct Foo { int x; Foo(int v) : x(v) {} };
```
```c
// C
void Foo_init(Foo *self, int v) { self->x = v; }
```

Destructor generation is required whenever a type holds a runtime-backed field
(`dpp_string`, `dpp_vector`, smart pointers, etc.).

---

## Function Name Mangling (Overloads)

Every function that could collide across separately compiled libraries must be
mangled. The hash is computed with **FNV-1a 32-bit** over the ASCII signature
string `return_type(param_type0,param_type1,...)` with no spaces. The lower 32
bits are rendered as 8 lowercase hex digits and appended to the function name
with `_`.

```
signature string:  "int(int)"       → FNV-1a 32-bit → e.g. a3f1b2c0
signature string:  "int(double)"    → FNV-1a 32-bit → e.g. 9d8e7f60
```

```cpp
// C++
int foo(int x);
int foo(double x);
```
```c
// C
int foo_a3f1b2c0(int x);
int foo_9d8e7f60(double x);
```

Call-sites are rewritten to the mangled name at the same pass that rewrites the
declaration. Because separate library compilations are expected, the mangling pass
should be applied to **all** non-`main` free functions, not only visible overload
sets within one translation unit.

---

## Inheritance

Single and multiple non-virtual inheritance lower by embedding the base as a named
field `base` (or `base0`, `base1`, ... for multiple bases) at the top of the derived
struct.

```cpp
// C++
struct Animal { int legs; };
struct Dog : Animal { int bark_volume; };
```
```c
// C
typedef struct Animal { int legs; } Animal;
typedef struct Dog { Animal base; int bark_volume; } Dog;
```

Base method calls and base field access are rewritten through the embedded `base`
field. Virtual inheritance and virtual methods are unsupported and rejected by the
syntax checker.

---

## Templates

### Function templates — single file

Simple function templates are **monomorphized**: one concrete C function is emitted
per distinct instantiation found in the translation unit. The generated name appends
the lowered type name(s) of the template arguments.

```cpp
// C++
template <typename T> T id(T x) { return x; }
id<int>(3);
id<double>(1.5);
```
```c
// C
int id_int(int x) { return x; }
double id_double(double x) { return x; }
```

### Function templates — multi-file / headers

When a function template lives in a header, each TU that instantiates it emits its
own concrete copy marked `static`. This avoids duplicate-symbol errors at link time,
at the cost of code duplication (the same trade-off C++ compilers make by default).

### Class templates

A class template is lowered by emitting one concrete struct per instantiation. For
simple type parameters the type name is appended directly. For compound or nested
type parameters the parameter list is hashed with FNV-1a 32-bit to keep the
generated name short and collision-free.

```cpp
// C++
template <typename T>
class Stack {
    T data[64];
    int top;
public:
    void push(T val);
    T pop();
};

Stack<int> a;
Stack<double> b;
Stack<std::pair<int, std::string>> c;   // compound → hashed
```
```c
// C — one struct + methods per instantiation
typedef struct Stack_int   { int    data[64]; int top; } Stack_int;
void Stack_int_push(Stack_int *self, int val);
int  Stack_int_pop(Stack_int *self);

typedef struct Stack_double { double data[64]; int top; } Stack_double;
void Stack_double_push(Stack_double *self, double val);
double Stack_double_pop(Stack_double *self);

// compound parameter: FNV-1a 32-bit over "pair<int,string>" → e.g. 4d3a9f01
typedef struct Stack_4d3a9f01 { dpp_pair_4d3a9f01 data[64]; int top; } Stack_4d3a9f01;
void Stack_4d3a9f01_push(Stack_4d3a9f01 *self, dpp_pair_4d3a9f01 val);
dpp_pair_4d3a9f01 Stack_4d3a9f01_pop(Stack_4d3a9f01 *self);
```

Each instantiation is emitted into the `.c` (or `.dh`) of the file where it first
appears. The transpiler must track which instantiations have already been emitted
to avoid duplicate definitions across included headers.

---

## Memory and Scope Tracking

The transpiler must track the lifetime of every runtime-backed local: smart pointers,
`std::string`, `std::vector`, `std::map`. At each return path, cleanup calls are
emitted in reverse declaration order before the `return` statement. Pointers are
initialized to `NULL` at the point of declaration so cleanup is safe on early-return
paths.

```cpp
// C++
void foo() {
    auto p = std::make_unique<Bar>();
}
```
```c
// C
void foo(void) {
    Bar *p = NULL;
    Bar_init(p);
    /* ... */
    if (p != NULL) { Bar_destroy(p); free(p); }
}
```

Scope-exit cleanup for non-return paths (nested blocks, early `break`, `goto`) is a
known gap; see `docs/unsupported.md`.

---

## Generated C Style

- Target **C11**.
- Emit `#include` directives explicitly. If a generated file uses `free()`, emit
  `#include <stdlib.h>` in that file even if `dpp_vector.h` happens to include it
  internally today — that internal include is an implementation detail and can
  change. Each generated file must declare every header it directly depends on.
- Use `NULL` not `0` for null pointers.
- Use `typedef struct Foo { ... } Foo;` (definition and typedef in one declaration).
- Prefer explicit function calls over macros; use macros only when a C function
  cannot express the needed behavior (e.g., `DPP_MIN`, `DPP_SORT_VECTOR`).
- Do not emit hidden heap allocation; allocation must be visible at the call site.
- Compactness wins over verbosity when there is a tradeoff.

---

## Inject Runtime Symbol Naming

All symbols in `inject/c/` use the `dpp_` prefix. Types: `dpp_TypeName`
(e.g., `dpp_string`, `dpp_vector_int`). Functions: `dpp_TypeName_method`
(e.g., `dpp_string_init`, `dpp_vector_int_push_back`).

Generated code `#include`s only the runtime headers it actually uses; the transpiler
decides which to emit using `used_*` flags returned by each conversion pass.
