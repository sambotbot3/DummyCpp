#include <iostream>

int square(int x) {
    return x * x;
}

int cube(int x) {
    return x * x * x;
}

int main() {
    std::cout << square(4) << std::endl;
    std::cout << cube(3) << std::endl;
    return 0;
}
