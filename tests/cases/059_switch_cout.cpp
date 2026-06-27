#include <iostream>

int main() {
    for (int i = 0; i < 3; ++i) {
        switch (i) {
            case 0: std::cout << "zero" << std::endl; break;
            case 1: std::cout << "one" << std::endl; break;
            default: std::cout << "other" << std::endl; break;
        }
    }
    return 0;
}
