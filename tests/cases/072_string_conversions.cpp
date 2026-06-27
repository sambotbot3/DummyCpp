#include <iostream>
#include <string>

int main() {
    std::string ns = "42";
    int n = std::stoi(ns);
    std::cout << n + 1 << std::endl;

    std::string hay = "hello world";
    if (hay.find("world") != std::string::npos) {
        std::cout << "found" << std::endl;
    }
    if (hay.find("xyz") == std::string::npos) {
        std::cout << "not found" << std::endl;
    }
    return 0;
}
