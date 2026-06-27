#include <iostream>
#include <string>

int main() {
    std::string a = "apple";
    std::string b = "banana";
    if (a == "apple") std::cout << "match" << std::endl;
    else std::cout << "no match" << std::endl;
    if (a != b) std::cout << "different" << std::endl;
    if (a < b) std::cout << "apple before banana" << std::endl;
    if (b > a) std::cout << "banana after apple" << std::endl;
    std::string c = "apple";
    if (a == c) std::cout << "equal strings" << std::endl;
    return 0;
}
