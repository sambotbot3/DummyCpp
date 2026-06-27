#include <iostream>
#include <string>

int main() {
    int n = 42;
    std::string s = std::to_string(n);
    std::cout << s << std::endl;

    double d = 3.14;
    std::string t = std::to_string(d);
    std::cout << t << std::endl;

    return 0;
}
