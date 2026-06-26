#include <iostream>

enum Color { Red, Green, Blue };

int main() {
    Color c = Green;
    std::cout << c << std::endl;
    if (c == Green) {
        std::cout << "is green" << std::endl;
    }
    return 0;
}
