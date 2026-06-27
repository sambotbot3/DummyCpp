#include <iostream>
#include <string>

std::string greet(const std::string &name) {
    return "Hello, " + name + "!";
}

std::string repeat(const std::string &s, int n) {
    std::string result = "";
    for (int i = 0; i < n; i++) {
        result = result + s;
    }
    return result;
}

int main() {
    std::string w = "world";
    std::string msg = greet(w);
    std::cout << msg.c_str() << std::endl;
    std::string r = repeat("ab", 3);
    std::cout << r.c_str() << std::endl;
    return 0;
}
