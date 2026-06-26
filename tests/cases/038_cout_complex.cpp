#include <iostream>

int main() {
    // String literal containing << (tests that the split doesn't break on it)
    std::cout << "a << b" << std::endl;
    // Parenthesised integer expression
    std::cout << (5 + 3) << std::endl;
    // Float literal
    std::cout << 3.14 << std::endl;
    return 0;
}
