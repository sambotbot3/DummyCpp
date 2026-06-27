#include <iostream>
#include <string>

std::string make_greeting(const std::string &name) {
    std::string result = "Hello, ";
    result += name;
    result += "!";
    return result;
}

std::string repeat(const std::string &s, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) {
        out += s;
    }
    return out;
}

int main() {
    std::string msg = make_greeting("world");
    std::cout << msg << std::endl;

    std::string r = repeat("ab", 3);
    std::cout << r << std::endl;

    return 0;
}
