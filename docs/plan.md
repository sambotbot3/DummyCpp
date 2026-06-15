# DummyCpp Initial Plan

## Goal

Build a C++ to C transpiler that starts with a deliberately small C++11 subset, emits compact but inspectable C, and progressively grows enough standard-library/runtime support to compile practical programs.

The early priority is a working compile pipeline, not perfect language-lawyer validation.

## Phase 0 - Project Shape

- Create a minimal C++ command-line tool named `dpp`.
- Accept an input `.cpp` file and emit a `.c` file.
- Use a Clang-based frontend for the initial implementation path.
- Add a tiny Dpp runtime directory later for generated helper functions and standard-library substitutes.
- Keep examples and tests tiny and runnable.

Suggested first layout:

```text
DummyCpp/
  AGENTS.md
  README.md
  docs/
  include/
  src/
  tests/
  examples/
```

## Phase 1 - First Vertical Slice

Target input:

```cpp
struct Point {
  int x;
  int y;
};

int add(int a, int b) {
  return a + b;
}

int main() {
  Point p{1, 2};
  return add(p.x, p.y);
}
```

Target output shape:

```c
typedef struct Point {
  int x;
  int y;
} Point;

int add(int a, int b) {
  return a + b;
}

int main(void) {
  Point p = (Point){1, 2};
  return add(p.x, p.y);
}
```

Acceptance:

- `dpp examples/point.cpp -o build/point.c`
- `cc build/point.c -o build/point`
- `./build/point` exits with `3`
- Generated C is always compiled as part of verification; tests should run the binary when possible.

## Phase 2 - Simple Methods And Name Lowering

Support simple class/struct methods by lowering `this` to an explicit pointer parameter.

Input:

```cpp
struct Counter {
  int value;
  void inc() { value += 1; }
};
```

Output shape:

```c
typedef struct Counter {
  int value;
} Counter;

void Counter_inc(Counter *self) {
  self->value += 1;
}
```

Initial constraints:

- No overloads at first, or use simple deterministic mangling.
- No access-control enforcement early.
- Treat `class` like `struct` until private/default access matters.

## Phase 3 - Constructors And Destructors

Lower simple constructors/destructors to explicit init/destroy functions.

- `T t(args);` becomes `T t; T_init(&t, args);`
- `T t{args};` becomes either compound literal if aggregate, or `T_init`.
- Destructors are inserted explicitly for simple local scopes once scope tracking exists.

Delay complicated RAII until the basic lifetime model is stable.

## Phase 4 - Tiny Standard Library Subset

Add Dpp runtime support incrementally for selected `std::` namespace features.

Initial candidates:

- `std::size_t` -> C `size_t`
- `std::nullptr_t` / `nullptr` -> `NULL` or `((void*)0)` depending context
- tiny `std::array<T, N>` -> struct wrapper around `T data[N]`
- tiny `std::span<T>` -> pointer + length
- tiny `std::string_view` -> pointer + length
- minimal `std::vector<T>` later with explicit runtime allocation helpers
- minimal `std::string` later

Rule: each standard-library feature gets a small compatibility note and compile/run examples.

## Phase 5 - Templates, Slowly

Only after normal structs/functions/methods work:

- Start with function templates used with concrete visible instantiations.
- Generate one C function per instantiation.
- Then simple class templates like `Array<T, N>`.
- Delay SFINAE, concepts, partial specialization, dependent name complexity, and heavy metaprogramming.

## Phase 6 - Larger Compatibility

Consider:

- namespaces,
- overload mangling,
- const/reference semantics,
- operator overloading,
- enum classes,
- lambdas without captures, then simple captures,
- header handling,
- multi-file builds.

## Non-Goals For Early Milestones

- Full C++ compliance.
- Full syntax/type checking.
- Exception support.
- ABI compatibility with C++ compilers.
- Drop-in replacement for Clang/GCC/MSVC.
- Full standard-library implementation.
