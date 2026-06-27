#include <iostream>
#include <string>

int main() {
    std::string s = "hello";
    std::cout << s[0] << std::endl;
    std::cout << s.at(4) << std::endl;
    s.push_back('!');
    std::cout << s << std::endl;
    return 0;
}
