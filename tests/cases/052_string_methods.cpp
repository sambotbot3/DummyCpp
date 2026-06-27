#include <iostream>
#include <string>

int main() {
    std::string s = "hello world";
    std::cout << s.length() << std::endl;
    std::size_t pos = s.find("world");
    if (pos != std::string::npos) {
        std::cout << pos << std::endl;
    }
    std::size_t pos2 = s.find("xyz");
    std::cout << (pos2 == std::string::npos ? 1 : 0) << std::endl;
    std::string sub = s.substr(0, 5);
    std::cout << sub << std::endl;
    return 0;
}
