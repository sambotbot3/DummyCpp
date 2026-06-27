#include <iostream>

int compute(int x) { return x * 2; }
int compute(int x, int y) { return x + y; }
double compute(double x) { return x * 3.14; }

int classify(int n) { return n > 0 ? 1 : -1; }
int classify(double d) { return d > 0.0 ? 2 : -2; }

int main() {
    std::cout << compute(5) << std::endl;
    std::cout << compute(3, 4) << std::endl;
    std::cout << compute(2.0) << std::endl;
    std::cout << classify(1) << std::endl;
    std::cout << classify(-1.5) << std::endl;
    return 0;
}
