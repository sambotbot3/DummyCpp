#include <iostream>
#include <string>

int greet(const std::string &name, int times = 1) {
    for (int i = 0; i < times; i++) {
        std::cout << "Hello " << name << std::endl;
    }
    return times;
}

int add(int x, int y = 10, int z = 100) {
    return x + y + z;
}

int main() {
    greet("Alice");
    greet("Bob", 2);
    std::cout << add(1) << std::endl;
    std::cout << add(1, 2) << std::endl;
    std::cout << add(1, 2, 3) << std::endl;
    return 0;
}
