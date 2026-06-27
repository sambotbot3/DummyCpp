#include <iostream>

// Custom destructor body that logs; scope-exit not tested here
// (dpp has no implicit RAII scope cleanup for record types yet).
struct Counter {
    int value;
    Counter(int v) : value(v) {}
    ~Counter() { value = -1; }
    int get() const { return value; }
};

int main() {
    Counter c(42);
    std::cout << c.get() << std::endl;
    return 0;
}
