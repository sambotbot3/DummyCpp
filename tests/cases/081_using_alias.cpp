#include <iostream>

using Integer = int;
using Index = int;

int main() {
    Integer x = 42;
    Index i = 0;
    for (i = 0; i < 3; ++i) {
        std::cout << x + i << std::endl;
    }
    return 0;
}
