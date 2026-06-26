#include <iostream>
#include <cstddef>

int main() {
    int *p = nullptr;
    if (p == nullptr) {
        std::cout << "null" << std::endl;
    }
    constexpr int limit = 100;
    std::cout << limit << std::endl;
    static_assert(limit > 0, "limit must be positive");
    return 0;
}
