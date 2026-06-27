#include <iostream>

const int MAX_VAL = 42;
const double E = 2.71828;
const char GRADE = 'A';

int add_max(int x) { return x + MAX_VAL; }

int main() {
    std::cout << MAX_VAL << std::endl;
    std::cout << E << std::endl;
    std::cout << GRADE << std::endl;
    std::cout << add_max(8) << std::endl;
    return 0;
}
