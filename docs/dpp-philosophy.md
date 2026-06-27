# Dpp intro
dpp may eventually dumb down c++ into a more user friendly approach.

It appears this can be done while respecting some of the axioms of a good language
- zero cost overhead
- have only one way to do things
- data oriented
- code readability
- push as much work to the compiler as possible


# Thinking out loud

## Templates
- Max / Min template
  - Type based on first variable or
  - Output type inferred from first variable

## Pointers & References
- Pointer working
  - Better period operator support
- Less distinction between reference and pointer
- Asserts for memory pointers

## Function Behavior

### Multiple Returns
- Multiple returns without pair, tuple, etc
- Compiler always looks for one return
- No namespace issues with multiple returns
C++
int, double foo(){
    return 1,2;
}
C
void foo(int* a, double* b){
    *a = 1;
    *b = 2;
}

### Return Value Lifetime
- Always generate a return value
- 
- C++ can generate functions without returns
- Transpiled c could always construct and pass return as parameter

## Arrays

### Subsects
- Easier partial passing.
C++
void bar(std::vector<float>& inv){}
vector<float> foo(5);
bar(foo[1:3]); // In compiler this becomes a pointer and size

### Safety
- Better array checks (safe keyword)
- Array checks outside loops
- Array subset passed into functions

## Safety Model

### Safe by Default
- Default safe mode enabled
- Must explicitly specify `unsafe`
- Does simple memory tracking and adds bound checks
- Always check pointers for non inline functions

### Memory Management
- Automatically deallocate memory when leaving scope
- Asserts for memory pointers

## Boolean Expressions

### Conditions
- Anything in a boolean field is treated as a comparison
  - Example: `if (x = 1)`

# C++ Shortcomings / Complaints

## Containers
- `std::list` add `[]` operator

## Headers
- Automatic header repeat-ignore (include guard behavior)

## Exceptions

### Allocation
- Exceptions require allocating exception memory at `try`
- Exception handling deallocates the stack

### Missing Coverage
- C++ doesn't handle raw pointer deallocation
- C++ doesn't handle divide-by-zero errors
- C++ just raises `SIGFPE` like C

### Alternatives
- Windows has C-style `try/except`

---

# Miscellaneous Language Ideas

## Namespace Behavior
- No namespace issues with multiple returns

## Compiler / Language Semantics
- Compiler always looks for one return

## Potential Keywords
- `safe`
- `unsafe`

## Strings
- represent strings as arrays
- allow easier conversion

int a = 5
string b=a;// compiler converts to to_string

