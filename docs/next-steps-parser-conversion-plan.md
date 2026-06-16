# DummyCpp Next Steps: Parser, Conversion, Runtime, And Tests

## Intent

DummyCpp should keep moving by vertical slices: a tiny C++ input compiles, transpiles to C, the generated C compiles, then both binaries produce identical stdout and exit status. The next sequence should make the internal architecture real enough to support classes and `std::vector` without becoming a full compiler project too early.

The guiding rule remains: build transformations that work on small C++11 programs first, then make parsing and syntax checks stricter as coverage grows.

## Near-Term Target

The next milestone should support:

- `#include <iostream>`, `std::cout`, `std::endl`
- free functions
- primitive local variables
- structs/classes with fields
- simple methods lowered to explicit `self` functions
- simple constructors lowered to explicit init functions
- direct member access and method calls
- `std::vector<int>` with `push_back`, `size`, index access, and destruction

The first class/vector examples should be intentionally small and boring. The test harness should keep comparing original C++ behavior against generated C behavior.

## Implementation Phases

### Phase A: Replace Text-Only Parser With A Token Layer

Keep `parser::ParsedSource` as the public return type, but expand it from raw text into a structured result:

```cpp
namespace dpp::parser {

enum class TokenKind {
  Identifier,
  Keyword,
  NumberLiteral,
  StringLiteral,
  CharLiteral,
  Operator,
  Punctuation,
  Preprocessor,
  Comment,
  EndOfFile,
  Unknown
};

enum class KeywordKind {
  None,
  Include,
  Using,
  Namespace,
  Struct,
  Class,
  Public,
  Private,
  Protected,
  Const,
  Static,
  Void,
  Bool,
  Char,
  Int,
  Long,
  Float,
  Double,
  Auto,
  Return,
  If,
  Else,
  For,
  While,
  Do,
  Break,
  Continue,
  Switch,
  Case,
  Default,
  New,
  Delete,
  This,
  Nullptr,
  Template,
  Typename,
  UsingNamespaceStdSpecial
};

struct Token {
  TokenKind kind;
  KeywordKind keyword;
  std::string text;
  SourceRange range;
};

struct ParsedSource {
  std::string text;
  std::vector<Token> tokens;
  std::vector<Diagnostic> diagnostics;
};

}
```

This lets early conversion modules stop splitting raw lines themselves. It also gives syntax checking a place to live without needing full AST correctness.

### Phase B: Keyword Sorting And Classification

The parser should classify words in this order:

1. Preprocessor lines: detect `#include`, `#define`, `#if`, `#ifdef`, `#ifndef`, `#endif`, `#pragma`. In early Dpp, accept `#include <iostream>` and selected standard headers, pass through or diagnose the rest.
2. Reserved C++ keywords: use a sorted/static lookup table from exact text to `KeywordKind`.
3. Standard namespace names: classify `std`, `cout`, `endl`, `vector`, `size_t`, `string`, `string_view` as identifiers plus semantic tags later, not as language keywords.
4. Dpp-supported library symbols: after tokenization, a semantic pass recognizes token sequences like `std :: cout`, `std :: vector < int >`, `using namespace std ;`.
5. User identifiers: anything not reserved remains an identifier.

Do not make `vector` a parser keyword. It is a library type. The parser should only identify the token sequence; the standard-library converter should decide whether Dpp supports that specialization.

Recommended lookup shape:

```cpp
struct KeywordEntry {
  std::string_view text;
  KeywordKind kind;
};

constexpr KeywordEntry kKeywords[] = {
  {"auto", KeywordKind::Auto},
  {"bool", KeywordKind::Bool},
  {"break", KeywordKind::Break},
  {"class", KeywordKind::Class},
  {"const", KeywordKind::Const},
  {"continue", KeywordKind::Continue},
  {"return", KeywordKind::Return},
  {"struct", KeywordKind::Struct},
  {"this", KeywordKind::This},
  {"void", KeywordKind::Void},
};
```

Keep it sorted and test lookup behavior. For now a simple binary search is enough; if the table grows, a generated perfect hash can come later.

### Phase C: Minimal Dpp IR

The conversion code should not all talk directly to Clang AST or raw tokens. Add a small project-owned intermediate representation.

Initial IR:

```cpp
namespace dpp::ir {

struct TranslationUnit {
  std::vector<IncludeDecl> includes;
  std::vector<UsingDecl> using_decls;
  std::vector<RecordDecl> records;
  std::vector<FunctionDecl> functions;
  std::vector<RuntimeNeed> runtime_needs;
};

struct RecordDecl {
  std::string name;
  bool was_class;
  std::vector<FieldDecl> fields;
  std::vector<MethodDecl> methods;
  std::vector<ConstructorDecl> constructors;
};

struct FunctionDecl {
  Type return_type;
  std::string name;
  std::vector<ParamDecl> params;
  CompoundStmt body;
};

}
```

This IR should start tiny. It does not need to represent all C++; it only needs the parts we can emit.

### Phase D: Clang Frontend Bridge

The Clang-based frontend should become the source of truth for parsing once the basic IR exists.

Plan:

- Keep the token parser for lightweight tests, keyword classification, and simple source diagnostics.
- Add `dpp::parser::parse_with_clang(path, options)` that builds a Clang AST for C++11.
- Add `dpp::parser::extract_supported_ir(ast)` that copies only supported constructs into Dpp IR.
- Unsupported AST nodes become clear diagnostics, not crashes.
- Conversion modules consume Dpp IR, not raw Clang nodes.

This avoids locking every converter to Clang APIs while still using Clang for real C++ parsing.

## Parser Responsibilities

The parser/syntax library should own:

- source file loading model and source locations
- lexical tokenization
- keyword lookup
- simple balanced delimiter checks
- preprocessor/include inventory
- Clang invocation setup
- conversion from supported Clang AST nodes to Dpp IR
- syntax/support diagnostics

It should not own:

- `std::cout` to `printf` lowering
- `std::vector` runtime layout decisions
- C emission formatting
- runtime injection

## Syntax Checker Responsibilities

Early syntax checks should be support checks, not full language checks.

Checks to add soon:

- reject unsupported headers except the known early set
- warn when `using namespace std;` is present but supported
- reject exceptions: `try`, `catch`, `throw`
- reject inheritance syntax initially: `class A : public B`
- reject virtual methods initially
- reject templates except `std::vector<T>` recognition
- reject lambdas initially
- reject references as fields initially; allow simple `const T&` function params later
- detect unsupported `cout` parts before emitting bad `printf`

Diagnostics should include file, line, column, severity, and a short suggestion when possible.

## Conversion Module Shape

Each conversion family should have a public header under `include/dpp/convert/` and an implementation under `src/convert/`.

Planned modules:

- `basic`: primitive types, `main`, returns, locals, aggregate init
- `iostream`: `std::cout`, `std::endl`, future `std::cerr`
- `records`: structs/classes, fields, methods, constructors
- `names`: namespaces and deterministic C symbol names
- `calls`: method calls, free function calls, member access
- `vector`: `std::vector<T>` recognition and runtime use
- `runtime`: collects runtime needs and injects C snippets

Converters should return both emitted C and metadata:

```cpp
struct ConvertResult {
  std::string source;
  std::vector<RuntimeNeed> runtime_needs;
  std::vector<Diagnostic> diagnostics;
};
```

## Class Lowering Plan

Treat `struct` and `class` the same for the first implementation except for recording `was_class`. Ignore private/public enforcement initially, but parse and drop access specifiers so generated C is clean.

Input:

```cpp
class Counter {
public:
  int value;
  Counter(int start) : value(start) {}
  void inc() { value += 1; }
  int get() const { return value; }
};
```

Output shape:

```c
typedef struct Counter {
  int value;
} Counter;

static inline void Counter_init(Counter *self, int start) {
  self->value = start;
}

static inline void Counter_inc(Counter *self) {
  self->value += 1;
}

static inline int Counter_get(const Counter *self) {
  return self->value;
}
```

Call lowering:

```cpp
Counter c(5);
c.inc();
std::cout << c.get() << std::endl;
```

becomes:

```c
Counter c;
Counter_init(&c, 5);
Counter_inc(&c);
printf("%d\n", Counter_get(&c));
```

Rules:

- non-const method: first arg `T *self`
- const method: first arg `const T *self`
- `this->x` and bare field `x` inside methods become `self->x`
- no overloads at first; reject duplicate method names
- constructor initializer lists support direct field initialization only
- destructor support waits until vector/local lifetime support exists

## Vector Lowering Plan

Use `inject/c/dpp_vector.h` and `inject/c/dpp_vector.c` as the first runtime-backed standard-library feature.

Initial supported C++:

```cpp
#include <vector>
#include <iostream>

int main() {
  std::vector<int> xs;
  xs.push_back(2);
  xs.push_back(5);
  std::cout << "size=" << xs.size() << std::endl;
  std::cout << "sum=" << xs[0] + xs[1] << std::endl;
  return xs.size();
}
```

Output shape:

```c
#include <stdio.h>
#include "dpp_vector.h"

int main(void) {
  dpp_vector xs;
  dpp_vector_init(&xs, sizeof(int));
  dpp_vector_push_back(&xs, &(int){2});
  dpp_vector_push_back(&xs, &(int){5});
  printf("%s%zu\n", "size=", dpp_vector_size(&xs));
  printf("%s%d\n", "sum=", *(int *)dpp_vector_at(&xs, 0) + *(int *)dpp_vector_at(&xs, 1));
  int dpp_return_value = (int)dpp_vector_size(&xs);
  dpp_vector_destroy(&xs);
  return dpp_return_value;
}
```

Runtime layout:

```c
typedef struct dpp_vector {
  void *data;
  size_t elem_size;
  size_t size;
  size_t capacity;
} dpp_vector;
```

Sam suggested void pointer plus two `size_t`s; I would add `elem_size` unless we generate one typed vector runtime per element type. `elem_size` keeps one reusable C runtime. If compactness becomes more important later, typed generated wrappers can remove it.

Required runtime functions:

- `dpp_vector_init(dpp_vector *v, size_t elem_size)`
- `dpp_vector_destroy(dpp_vector *v)`
- `dpp_vector_size(const dpp_vector *v)`
- `dpp_vector_data(dpp_vector *v)`
- `dpp_vector_at(dpp_vector *v, size_t index)`
- `dpp_vector_push_back(dpp_vector *v, const void *elem)`
- internal grow helper

Early constraints:

- support `std::vector<int>` first
- then `std::vector<double>`
- then `std::vector<T>` for simple POD records
- no iterators yet
- no range-for yet
- no allocator support
- no exception behavior; runtime should abort or return error for allocation failure depending project decision

## Runtime Injection Plan

Generated C should only include runtime pieces it needs.

Add:

```cpp
enum class RuntimeNeed {
  Stdio,
  Vector
};
```

The transpiler should:

1. collect runtime needs from conversion passes
2. emit system includes first
3. emit Dpp runtime includes second
4. copy/compile needed runtime `.c` files during tests

For `test_all.sh`, when generated code uses vector, compile with:

```bash
cc generated.c inject/c/dpp_vector.c -I inject/c -o generated
```

Longer term, create `build/generated/<case>/` and copy runtime snippets next to generated C for standalone inspection.

## Iostream Improvements Needed Before Vector Tests

`iostream` lowering needs basic type awareness. Current fallback assumes non-string expressions are `%d`.

Next support:

- string literal -> `%s`
- `int` -> `%d`
- `size_t` -> `%zu`
- `double` -> `%g`
- `char` -> `%c`
- `bool` -> `%s` with ternary or `%d` initially
- `std::endl` -> newline in the same `printf`
- multiple `cout` lines remain multiple `printf` calls
- one `cout` chain remains one `printf`

For vector, `xs.size()` must choose `%zu`, while `xs[0]` for `vector<int>` chooses `%d`.

## Next Minimal Examples

Add these in order. Each should be a test case under `tests/cases/` and may also have a clearer example under `examples/`.

### 003_locals_and_arithmetic.cpp

Purpose: make sure expression handling is not only function calls.

```cpp
#include <iostream>

int main() {
  int a = 2;
  int b = 5;
  int c = a * b + 3;
  std::cout << "c=" << c << std::endl;
  return c;
}
```

### 004_class_counter.cpp

Purpose: first `class`, methods, constructor, method-call lowering.

```cpp
#include <iostream>

class Counter {
public:
  int value;
  Counter(int start) : value(start) {}
  void inc() { value += 1; }
  int get() const { return value; }
};

int main() {
  Counter c(4);
  c.inc();
  std::cout << "counter=" << c.get() << std::endl;
  return c.get();
}
```

### 005_struct_method.cpp

Purpose: method lowering without constructor complexity.

```cpp
#include <iostream>

struct Point {
  int x;
  int y;
  int sum() const { return x + y; }
};

int main() {
  Point p{3, 6};
  std::cout << "sum=" << p.sum() << std::endl;
  return p.sum();
}
```

### 006_vector_int.cpp

Purpose: first runtime-injected `std::vector`.

```cpp
#include <iostream>
#include <vector>

int main() {
  std::vector<int> xs;
  xs.push_back(2);
  xs.push_back(5);
  std::cout << "size=" << xs.size() << std::endl;
  std::cout << "sum=" << xs[0] + xs[1] << std::endl;
  return xs.size();
}
```

### 007_vector_record.cpp

Purpose: prove vector runtime can hold simple structs after `vector<int>` works.

```cpp
#include <iostream>
#include <vector>

struct Point {
  int x;
  int y;
};

int main() {
  std::vector<Point> pts;
  pts.push_back(Point{1, 2});
  pts.push_back(Point{3, 4});
  std::cout << "x=" << pts[1].x << std::endl;
  return pts[0].x + pts[1].y;
}
```

This may require lowering `pts[1].x` to `((Point *)dpp_vector_at(&pts, 1))->x` or a temporary pointer.

## Test Harness Roadmap

Keep `scripts/test_all.sh`, but make it more comprehensive:

- print each case name
- build original C++ with `c++ -std=c++11`
- transpile with `dpp`
- compile generated C with all required runtime files
- capture stdout and exit code for both
- diff stdout
- compare exit code
- preserve generated C and logs under `build/tests/<case>/`
- support expected-failure tests under `tests/unsupported/`

Suggested directory:

```text
tests/
  cases/
    001_cout.cpp
    002_struct_return.cpp
    003_locals_and_arithmetic.cpp
    004_class_counter.cpp
    005_struct_method.cpp
    006_vector_int.cpp
    007_vector_record.cpp
  unsupported/
    exceptions.cpp
    inheritance.cpp
    templates_user_defined.cpp
```

Unsupported tests should pass when Dpp emits a clear diagnostic and exits nonzero.

## Concrete Next Work Order

Progress so far:

- Added parser token structs and sorted keyword lookup.
- Added records conversion module: `include/dpp/convert/records.h`.
- Implemented class constructor/method lowering for `004_class_counter.cpp`.
- Implemented struct method extraction/lowering for `005_struct_method.cpp`.
- Added runtime-aware harness linking for generated C that includes `dpp_vector.h`.
- Finished the first `dpp_vector` runtime functions.
- Implemented `std::vector<int>` recognition and lower `006_vector_int.cpp`.
- Implemented vector of POD record support for `007_vector_record.cpp`.
- Added `using namespace std; vector<int>` support in `008_using_namespace_vector.cpp`.
- Added `std::vector<double>` support in `009_vector_double.cpp`.
- Added `push_back(existing_record)` support in `010_vector_record_variable.cpp`.
- Added parameterized class method support in `011_class_method_param.cpp`.
- Added expected-failure tests for unsupported headers, exceptions, and inheritance.

Next work:

1. Add explicit parser/keyword unit tests instead of only exercising tokenization indirectly.
2. Move `cout` parsing from line splitting toward token-aware chain parsing.
3. Add stronger type tracking for expressions used in `cout`.
4. Replace more bootstrap string passes with IR-driven emission.
5. Expand unsupported-feature diagnostics for lambdas, references-as-fields, overload collisions, and unsupported vector element types.
6. Add Clang AST bridge once the IR shape is proven by more class/vector cases.

## Open Questions To Track

Move answers into `docs/decisions.md` when settled.

- Should Dpp generated C include runtime headers by relative path, or copy runtime snippets next to generated output?
- For allocation failure in runtime vector, should Dpp call `abort()`, return an error code, or expose both modes?
- Should generated helper functions be `static inline` by default or normal external functions?
- Should method names mangle as `Type_method` first, or should we immediately use a collision-resistant scheme?
- Should Dpp support only `std::vector<T>` spelling first, or also `using namespace std; vector<T>` in the same milestone?
