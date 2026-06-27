#include <iostream>
#include <string>

int main() {
    std::string a = "hello";
    std::string b = " world";
    std::string c;
    c = a + b;
    std::cout << c << std::endl;
    c = "prefix: " + a;
    std::cout << c << std::endl;
    c = a + " " + std::to_string(42);
    std::cout << c << std::endl;
    return 0;
}
