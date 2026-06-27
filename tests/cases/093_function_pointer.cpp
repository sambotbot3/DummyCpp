#include <iostream>

int apply(int x, int (*fn)(int)) { return fn(x); }
int double_it(int x) { return x * 2; }
int square(int x) { return x * x; }

int main() {
    std::cout << apply(5, double_it) << std::endl;
    std::cout << apply(4, square) << std::endl;
    int (*fn)(int) = double_it;
    std::cout << fn(7) << std::endl;
    return 0;
}
