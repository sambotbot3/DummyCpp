#include <iostream>
#include <string>

int main() {
    std::string a = "hello";
    std::string b = " world";
    std::string c = a + b;
    std::cout << c << std::endl;

    std::string d = "count: " + std::to_string(42);
    std::cout << d << std::endl;

    std::string e = "x=";
    e += a;
    e += "!";
    std::cout << e << std::endl;

    std::string f = "n=" + std::to_string(7) + " end";
    std::cout << f << std::endl;

    return 0;
}
